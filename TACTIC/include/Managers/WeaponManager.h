#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Gets the type of item equipped in an actor's hand as an integer.
/// </summary>
/// <param name="obj">Object in a specific hand.</param>
/// <returns>Integer representation of the type of item held in an actor's hand.</returns>
int GetHandType(RE::TESForm* obj);

/// <summary>
/// Converts the integer representation of an equipped item type to a human-readable string.
/// </summary>
/// <param name="type">Integer representation of an equipped item type</param>
/// <returns>String identifier of an equipped item type.</returns>
std::string_view GetHandTypeName(int type);

/// <summary>
/// Gets the category of a spell.
/// </summary>
std::string_view GetSpellCategoryName(RE::TESForm* obj);

/// <summary>
/// Gest what a hand slot holds.
/// </summary>
std::string_view GetHandContentName(RE::TESForm* obj);

/// <summary>
/// Classifies a just-equipped item for data collection.
/// </summary>
/// <param name="actor">Actor the item was equipped on.</param>
/// <param name="form">Item that was just equipped.</param>
/// <param name="inLeftHand">Which hand's slot the item is being classified for.</param>
/// <returns>Name of the InventoryAction, or empty if none applies.</returns>
std::string GetEquipActionName(RE::Actor* actor, RE::TESForm* form, bool inLeftHand);

/// <summary>
/// Classifies what currently occupies one of an actor's hand slots.
/// </summary>
/// <param name="actor">Actor whose hand to check.</param>
/// <param name="leftHand">True for the left hand, false for the right.</param>
/// <returns>The hand's current equip-collection label.</returns>
std::string GetHandSlotLabel(RE::Actor* actor, bool leftHand);

/// <summary>
/// Which magic schools an actor knows at least one spell in.
/// </summary>
struct KnownSpellSchools
{
    // Destruction is split by delivery mechanism.
    bool destructionContinuous = false;  // continuoys, e.g. Flames
    bool destructionProjectile = false;  // fire-and-forget aimed, e.g. Firebolt
    bool destructionRune = false;        // target-location, e.g. Fire Rune
    bool destructionCloak = false;       // Cloaks, e.g. Flame Cloak

    // Armor/Flesh-type Alteration spells.
    bool armor = false;

    // Restoration is split into healing and warding.
    bool restorationHealing = false;
    bool restorationWard = false;

    // Conjuration is split into summons and weapons.
    bool conjurationFollower = false;
    bool conjurationWeapon = false;
};

/// <summary>
/// Checks which magic schools an actor knows at least one spell in.
/// </summary>
/// <param name="actor">Actor to check.</param>
/// <returns>Set of known magic schools.</returns>
KnownSpellSchools GetKnownSpellSchools(RE::Actor* actor);

/// <summary>
/// Keeps track of what kind of weapons the actor has in their inventory using broad categoreis.
/// </summary>
struct InventoryWeaponFlags
{
    bool oneHandedMelee = false;   // Sword/Dagger/Axe/Mace
    bool twoHandedMelee = false;   // Greatsword/Battleaxe/Warhammer
    bool oneHandedRanged = false;  // Staff
    bool twoHandedRanged = false;  // Bow/Crossbow
    bool shield = false;

    // String representations of arrow and bolt counts.
    std::string_view arrowCountLevel = "None";
    std::string_view boltCountLevel = "None";
};

/// <summary>
/// Getter for InventoryWeaponFlags.
/// </summary>
InventoryWeaponFlags GetInventoryWeaponFlags(RE::Actor* actor);

/// <summary>
/// Counts an actor's raw ammo total — arrows if bolt is false, bolts if true.
/// </summary>
/// <param name="actor">Actor to check for.</param>
/// <param name="bolt">True for checking for bolts. False for checking for arrows.</param>
/// <returns>Arrow or bolt count.</returns>
int CountAmmo(RE::Actor* actor, bool bolt);

