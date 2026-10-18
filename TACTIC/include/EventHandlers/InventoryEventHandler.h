#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Used to keep inventory-related cached state up to date. Triggered whenever items are moved between containers.
/// </summary>
class InventoryEventHandler :
    public RE::BSTEventSink<RE::TESContainerChangedEvent>
{
public:
    static InventoryEventHandler* GetSingleton();

    RE::BSEventNotifyControl ProcessEvent
    (
        const RE::TESContainerChangedEvent* event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>* source
    ) override;
};