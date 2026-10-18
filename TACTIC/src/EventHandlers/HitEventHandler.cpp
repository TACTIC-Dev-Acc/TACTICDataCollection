#include "EventHandlers/HitEventHandler.h"

#include "Managers/ActorContextManager.h"
#include "Managers/ModActivationManager.h"

HitEventHandler* HitEventHandler::GetSingleton()
{
    static HitEventHandler singleton;
    return std::addressof(singleton);
}

// Used to find the last actor that "self" landed a hit on
RE::BSEventNotifyControl HitEventHandler::ProcessEvent
(
    const RE::TESHitEvent* event,
    RE::BSTEventSource<RE::TESHitEvent>*)
{
    if (not event) return RE::BSEventNotifyControl::kContinue;
    if (not IsModActive()) return RE::BSEventNotifyControl::kContinue;

    // Get target
    auto* target = event->target.get();

    if (not target) return RE::BSEventNotifyControl::kContinue;

    // Convert target to cator
    auto* targetActor = skyrim_cast<RE::Actor*>(target);

    if (not targetActor) return RE::BSEventNotifyControl::kContinue;

    // Get aggressor
    auto* aggressorActor = skyrim_cast<RE::Actor*>(event->cause.get());

    // Update lastHitActor in ActorContext
    if (aggressorActor)
    {
        GetActorContext(aggressorActor).lastHitActor = targetActor->GetFormID();
    }

    return RE::BSEventNotifyControl::kContinue;
}