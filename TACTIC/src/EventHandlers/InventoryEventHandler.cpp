#include "EventHandlers/InventoryEventHandler.h"

#include "Managers/ModActivationManager.h"
#include "Managers/ResourceRestorationManager.h"
#include "Managers/SettingsManager.h"

InventoryEventHandler* InventoryEventHandler::GetSingleton()
{
    static InventoryEventHandler singleton;
    return std::addressof(singleton);
}

RE::BSEventNotifyControl InventoryEventHandler::ProcessEvent
(
    const RE::TESContainerChangedEvent* event,
    RE::BSTEventSource<RE::TESContainerChangedEvent>*)
{
    if (!event) return RE::BSEventNotifyControl::kContinue;

    // Also keeps scrolls from being removed from the player's inventory while the mod is dormant
    if (not IsModActive()) return RE::BSEventNotifyControl::kContinue;

    auto* actor = RE::PlayerCharacter::GetSingleton();

    if (not actor) return RE::BSEventNotifyControl::kContinue;

    // Scrolls are weird and too niche, therefore they add unnecessary complexity to training.
    // The player can turn them back on with "scrolls_enabled" in TacticSettings.json, in which case they are left alone
    if (not AreScrollsEnabled() and event->newContainer == actor->GetFormID())
    {
        if (auto* form = RE::TESForm::LookupByID(event->baseObj); form) 
        {
            if (auto* scroll = form->As<RE::ScrollItem>()) 
            {
                // Remove a scroll immediately after in enters the actor's inventory
                actor->RemoveItem(scroll, event->itemCount, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                return RE::BSEventNotifyControl::kContinue;
            }
        }
    }

    // Initializes the actors resotative item information
    InitRestoratives(actor);

    return RE::BSEventNotifyControl::kContinue;
}