#include "Managers/ArmorManager.h"

std::string_view GetBodyArmorType(RE::Actor* actor) 
{
    if (!actor) return "Unknown";

    auto* body = actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kBody);
    if (!body) return "Naked";

    static auto* heavyKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ArmorHeavy");
    static auto* lightKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ArmorLight");

    if (heavyKeyword && body->HasKeyword(heavyKeyword)) return "Heavy";
    if (lightKeyword && body->HasKeyword(lightKeyword)) return "Light";

    return "Clothing";
}
