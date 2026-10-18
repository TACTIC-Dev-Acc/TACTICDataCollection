#include <algorithm>

#include "Managers/ActorContextManager.h"
#include "Managers/ResourceRestorationManager.h"

RestoreType ClassifyRestorative(RE::AlchemyItem* item, float& outMagnitude, float& outDuration) 
{
    outMagnitude = 0.0f;
    outDuration = 0.0f;

    if (!item || item->IsPoison()) return RestoreType::kNone;

    for (auto* effect : item->effects) 
    {
        if (!effect || !effect->baseEffect) continue;

        const auto& data = effect->baseEffect->data;

        using Flag = RE::EffectSetting::EffectSettingData::Flag;

        if (data.flags.any(Flag::kDetrimental)) continue;
        if (data.flags.any(Flag::kRecover)) continue;

        RestoreType type = RestoreType::kNone;
        switch (data.primaryAV) 
        {
            case RE::ActorValue::kHealth:
                type = RestoreType::kHealth;
                break;
            case RE::ActorValue::kStamina:
                type = RestoreType::kStamina;
                break;
            case RE::ActorValue::kMagicka:
                type = RestoreType::kMagicka;
                break;
            default:
                break;  // Ignore regeneration types
        }
        if (type == RestoreType::kNone) continue;

        outMagnitude = effect->effectItem.magnitude;
        outDuration = static_cast<float>(effect->effectItem.duration);
        return type;
    }

    return RestoreType::kNone;
}

void InitRestoratives(RE::Actor* actor)
{
    if (!actor) return;

    RestoreItems& inv = GetActorContext(actor).restoratives;

    inv.health.clear();
    inv.stamina.clear();
    inv.magicka.clear();

    for (const auto& [form, entry] : actor->GetInventoryCounts()) 
    {
        const auto count = entry;
        if (count <= 0) continue;

        auto* potion = form->As<RE::AlchemyItem>();
        if (!potion) continue;

        float mag = 0.0f, dur = 0.0f;
        const RestoreType type = ClassifyRestorative(potion, mag, dur);
        if (type == RestoreType::kNone) continue;

        RestorativeItem ri;
        ri.item = potion;
        ri.magnitude = mag;
        ri.duration = dur;
        ri.isFood = potion->IsFood();
        ri.count = static_cast<uint32_t>(count);

        inv.For(type).push_back(ri);
    }

    // Best first: instant items ahead of over-time, then by effective restore.
    auto rank = [](const RestorativeItem& a, const RestorativeItem& b) 
        {
        if (a.IsInstant() != b.IsInstant()) return a.IsInstant();
        return a.EffectiveRestore() > b.EffectiveRestore();
    };
    std::sort(inv.health.begin(), inv.health.end(), rank);
    std::sort(inv.stamina.begin(), inv.stamina.end(), rank);
    std::sort(inv.magicka.begin(), inv.magicka.end(), rank);

    inv.initialized = true;
}

bool HasRestorative(RE::Actor* actor, RestoreType type)
{
    if (!actor) return false;
    RestoreItems& inv = GetActorContext(actor).restoratives;
    if (!inv.initialized) InitRestoratives(actor);
    return !inv.For(type).empty();
}
