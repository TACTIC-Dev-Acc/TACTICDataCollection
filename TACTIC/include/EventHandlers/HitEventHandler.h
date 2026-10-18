#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Custom handler for TESHIT event. Triggered when an actor hits another actor.
/// </summary>
class HitEventHandler :
    public RE::BSTEventSink<RE::TESHitEvent>
{
    public:
        static HitEventHandler* GetSingleton();

        RE::BSEventNotifyControl ProcessEvent
        (
            const RE::TESHitEvent* event,
            RE::BSTEventSource<RE::TESHitEvent>* source
        ) override;
};