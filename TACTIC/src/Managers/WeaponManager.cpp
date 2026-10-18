#include "Managers/WeaponManager.h"

namespace 
{
    int GetSpellSchoolType(RE::MagicItem* spell) 
    {
        if (!spell) return 18;  // spell with no determinable school
        switch (spell->GetAssociatedSkill())
        {
            case RE::ActorValue::kDestruction: return 10;
            case RE::ActorValue::kRestoration: return 11;
            case RE::ActorValue::kAlteration:  return 12;
            case RE::ActorValue::kIllusion:    return 13;
            case RE::ActorValue::kConjuration: return 14;
            default:                           return 18;
        }
    }

    // Destruction is split by delivery mechanism
    void ClassifyDestruction(RE::SpellItem* spell, KnownSpellSchools& out)
    {
        using CastingType = RE::MagicSystem::CastingType;
        using Delivery = RE::MagicSystem::Delivery;
        using Archetype = RE::EffectSetting::Archetype;

        if (spell->GetCastingType() == CastingType::kConcentration) 
        {
            out.destructionContinuous = true;
            return;
        }

        if (spell->GetDelivery() == Delivery::kTargetLocation) 
        {
            out.destructionRune = true;
            return;
        }

        if (spell->GetDelivery() == Delivery::kSelf)
        {
            for (auto* effect : spell->effects)
            {
                if (effect && effect->baseEffect && effect->baseEffect->HasArchetype(Archetype::kCloak))
                {
                    out.destructionCloak = true;
                    return;
                }
            }
            return;
        }

        if (spell->GetDelivery() == Delivery::kAimed) out.destructionProjectile = true;
    }

    // Restoration is split by what the effect actually does
    void ClassifyRestoration(RE::SpellItem* spell, KnownSpellSchools& out) 
    {
        static auto* wardKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicWard");

        using Flag = RE::EffectSetting::EffectSettingData::Flag;

        for (auto* effect : spell->effects) 
        {
            if (!effect || !effect->baseEffect) continue;
            auto* baseEffect = effect->baseEffect;

            if (wardKeyword && baseEffect->HasKeyword(wardKeyword))
            {
                out.restorationWard = true;
                continue;
            }

            if (baseEffect->data.primaryAV == RE::ActorValue::kHealth && !baseEffect->data.flags.any(Flag::kDetrimental))
            {
                out.restorationHealing = true;
            }
        }
    }

    // Conjuration effects split by archetype.
    void ClassifyConjuration(RE::SpellItem* spell, KnownSpellSchools& out) 
    {
        using Archetype = RE::EffectSetting::Archetype;

        for (auto* effect : spell->effects)
        {
            if (!effect || !effect->baseEffect) continue;

            switch (effect->baseEffect->GetArchetype()) 
            {
                case Archetype::kSummonCreature:
                case Archetype::kReanimate:
                    out.conjurationFollower = true;
                    break;
                case Archetype::kBoundWeapon:
                    out.conjurationWeapon = true;
                    break;
                default:
                    break;
            }
        }
    }

    // Alteration is narrowed to just armor/Flesh-type spells.
    void ClassifyAlteration(RE::SpellItem* spell, KnownSpellSchools& out)
    {
        static auto* armorKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicArmorSpell");
        if (!armorKeyword) return;

        for (auto* effect : spell->effects)
        {
            if (effect && effect->baseEffect && effect->baseEffect->HasKeyword(armorKeyword)) 
            {
                out.armor = true;
                return;
            }
        }
    }

    std::string_view BucketAmmoCount(int count)
    {
        if (count == 0) return "None";
        if (count < 10) return "Critical";
        if (count < 20) return "Low";
        if (count < 50) return "Medium";
        return "High";
    }

}

int GetHandType(RE::TESForm* obj) 
{
    if (!obj) return 0;  // Empty

    if (auto* spell = obj->As<RE::SpellItem>()) return GetSpellSchoolType(spell);

    if (auto* armor = obj->As<RE::TESObjectARMO>())
        if (armor->IsShield()) return 16;

    if (auto weapon = obj->As<RE::TESObjectWEAP>()) return static_cast<int>(weapon->GetWeaponType());

    return 17;
}

// Collapsed to a small generic set.
std::string_view GetHandTypeName(int type)
{
    switch (type)
    {
        case 0: return "Empty";

        // One-handed melee (Sword/Dagger/Axe/Mace)
        case 1:
        case 2:
        case 3:
        case 4:
            return "OneHandedMelee";

        // Two-handed melee
        case 5:
        case 6:
            return "TwoHandedMelee";

        // Bow/Crossbow
        case 7:
        case 9:
            return "TwoHandedRanged";

        // Staff
        case 8:
            return "OneHandedRanged";

        // Spells
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 18:
            return "Spell";

        case 16: return "Shield";

        case 17: return "Other";

        default: return "Invalid";
    }
}

std::string_view GetSpellCategoryName(RE::TESForm* obj)
{
    auto* spell = obj ? obj->As<RE::SpellItem>() : nullptr;
    if (!spell) return "None";

    KnownSpellSchools flags;

    switch (spell->GetAssociatedSkill()) 
    {
        // Any Destruction spell is a damage spell
        case RE::ActorValue::kDestruction:
            return "DamageSpell";

        case RE::ActorValue::kRestoration:
            ClassifyRestoration(spell, flags);
            if (flags.restorationHealing) return "HealingSpell";
            if (flags.restorationWard) return "WardSpell";
            break;

        case RE::ActorValue::kAlteration:
            ClassifyAlteration(spell, flags);
            if (flags.armor) return "ArmorSpell";
            break;

        case RE::ActorValue::kConjuration:
            ClassifyConjuration(spell, flags);
            if (flags.conjurationFollower) return "ConjurationFollowerSpell";
            if (flags.conjurationWeapon) return "ConjurationWeaponSpell";
            break;

        default:
            break;
    }

    return "OtherSpell";
}

std::string_view GetHandContentName(RE::TESForm* obj)
{
    if (obj and obj->As<RE::SpellItem>()) return GetSpellCategoryName(obj);
    return GetHandTypeName(GetHandType(obj));
}

std::string GetEquipActionName(RE::Actor* actor, RE::TESForm* form, bool inLeftHand) 
{
    if (!actor || !form) return "";

    if (form->As<RE::SpellItem>()) return inLeftHand ? "SwitchToSpellLeft" : "SwitchToSpellRight";

    switch (GetHandType(form))
    {
        // Bow/Crossbow/Staff
        case 7:
        case 9:
            return "SwitchToRangedBoth";

        case 8:
            return inLeftHand ? "SwitchToRangedLeft" : "SwitchToRangedRight";

        // One-handed mele
        case 1:
        case 2:
        case 3:
        case 4:
            return inLeftHand ? "SwitchToMeleeLeft" : "SwitchToMeleeRight";

        // Two-handed melee
        case 5:
        case 6:
            return "SwitchToMeleeBoth";

        // Shield
        case 16:
            return "SwitchToShield";

        // Empty, Unknown
        default:
            return "";
    }
}

std::string GetHandSlotLabel(RE::Actor* actor, bool leftHand)
{
    if (!actor) return "";

    RE::TESForm* form = actor->GetEquippedObject(leftHand);
    if (!form) return leftHand ? "EquipUnarmedLeft" : "EquipUnarmedRight";

    return GetEquipActionName(actor, form, leftHand);
}

KnownSpellSchools GetKnownSpellSchools(RE::Actor* actor) 
{
    KnownSpellSchools schools;
    if (!actor) return schools;

    class Collector : public RE::Actor::ForEachSpellVisitor 
    {
    public:
        explicit Collector(KnownSpellSchools& out) : out(out) {}

        RE::BSContainer::ForEachResult Visit(RE::SpellItem* spell) override 
        {
            if (spell) 
            {

                switch (spell->GetAssociatedSkill())
                {
                    case RE::ActorValue::kDestruction: ClassifyDestruction(spell, out); break;
                    case RE::ActorValue::kRestoration: ClassifyRestoration(spell, out); break;
                    case RE::ActorValue::kAlteration:  ClassifyAlteration(spell, out); break;
                    case RE::ActorValue::kConjuration: ClassifyConjuration(spell, out); break;
                    default: break;
                }
            }
            return RE::BSContainer::ForEachResult::kContinue;
        }

    private:
        KnownSpellSchools& out;
    };

    Collector collector{schools};
    actor->VisitSpells(collector);
    return schools;
}

InventoryWeaponFlags GetInventoryWeaponFlags(RE::Actor* actor) 
{
    InventoryWeaponFlags flags;
    if (!actor) return flags;

    RE::TESForm* leftEquipped = actor->GetEquippedObject(true);
    RE::TESForm* rightEquipped = actor->GetEquippedObject(false);

    int arrowCount = 0;
    int boltCount = 0;

    for (const auto& [form, entry] : actor->GetInventoryCounts()) 
    {
        if (entry <= 0) continue;

        if (auto* ammo = form->As<RE::TESAmmo>()) {
            (ammo->IsBolt() ? boltCount : arrowCount) += entry;
            continue;
        }

        // An already-equipped weapon can't be "switched to" again
        bool isUnequipped = form != leftEquipped and form != rightEquipped;

        switch (GetHandType(form)) 
        {
            case 1: case 2: case 3: case 4:
                if (isUnequipped) flags.oneHandedMelee = true;
                break;
            case 5: case 6:
                if (isUnequipped) flags.twoHandedMelee = true;
                break;
            case 8:
                if (isUnequipped) flags.oneHandedRanged = true;
                break;
            case 7: case 9:
                if (isUnequipped) flags.twoHandedRanged = true;
                break;
            case 16:
                flags.shield = true;
                break;
            default:
                break;
        }
    }

    flags.arrowCountLevel = BucketAmmoCount(arrowCount);
    flags.boltCountLevel = BucketAmmoCount(boltCount);
    return flags;
}

int CountAmmo(RE::Actor* actor, bool bolt)
{
    if (!actor) return 0;

    int total = 0;
    for (const auto& [form, entry] : actor->GetInventoryCounts())
    {
        if (entry <= 0) continue;

        auto* ammo = form->As<RE::TESAmmo>();
        if (ammo && ammo->IsBolt() == bolt) total += entry;
    }
    return total;
}
