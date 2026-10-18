#include "EventHandlers/EquipEventHandler.h"

#include "DataCollection.h"
#include "Managers/ModActivationManager.h"

EquipEventHandler* EquipEventHandler::GetSingleton()
{
    static EquipEventHandler singleton;
    return std::addressof(singleton);
}

RE::BSEventNotifyControl EquipEventHandler::ProcessEvent
(
    const RE::TESEquipEvent* event,
    RE::BSTEventSource<RE::TESEquipEvent>*)
{
    if (not event) return RE::BSEventNotifyControl::kContinue;
    if (not IsModActive()) return RE::BSEventNotifyControl::kContinue;

    auto* self = RE::PlayerCharacter::GetSingleton();

    if (not self) return RE::BSEventNotifyControl::kContinue;

    // Only run for the actor from the point of view of which the data is collected
    if (event->actor.get() != self) return RE::BSEventNotifyControl::kContinue;

    // Consumables: a burst of potions is debounced into one snapshot rather than one per item.
    if (event->equipped) 
    {
        auto* form = RE::TESForm::LookupByID(event->baseObject);

        if (auto* item = form ? form->As<RE::AlchemyItem>() : nullptr) 
        {
            float magnitude = 0.0f;
            float duration = 0.0f;

            // Classify consumable
            RestoreType type = ClassifyRestorative(item, magnitude, duration);

            // Skip items that could not be classified
            if (type != RestoreType::kNone) NotifyRestorativeConsumed(self, type);

            return RE::BSEventNotifyControl::kContinue;
        }
    }

    // Weapon/spell/staff/bow/shield hand changes.
    NotifyHandsChanged(self);

    return RE::BSEventNotifyControl::kContinue;
}
