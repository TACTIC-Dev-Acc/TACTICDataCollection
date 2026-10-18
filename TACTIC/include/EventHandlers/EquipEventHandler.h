#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Custom handler for TESEquipEvent. Used to detect when the player consumes a restorative potion/food item.
/// </summary>
class EquipEventHandler :
    public RE::BSTEventSink<RE::TESEquipEvent>
{
    public:
        static EquipEventHandler* GetSingleton();

        RE::BSEventNotifyControl ProcessEvent
        (
            const RE::TESEquipEvent* event,
            RE::BSTEventSource<RE::TESEquipEvent>* source
        ) override;
};
