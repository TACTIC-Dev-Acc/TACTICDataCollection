#include "Managers/ModActivationManager.h"

#include <chrono>
#include <string_view>

#include "Managers/ActorContextManager.h"
#include "Managers/ResourceRestorationManager.h"
#include "Managers/ShoutSuppressionManager.h"
#include "Managers/WeaponManager.h"

#include "DataCollection.h"
#include "Misc.h"

namespace {
    // "Unbound" (MQ101), the Helgen intro quest, in Skyrim.esm
    constexpr RE::FormID kUnboundQuest = 0x03372B;

    // The stage that gives "Find some equipment" (Hadvar) / "Loot Gunjar's body" (Ralof).
    constexpr std::uint16_t kFreedStage = 255;

    constexpr float kCheckIntervalSeconds = 0.25f;

    bool g_active = false;
    float g_checkCooldown = 0.0f;
    bool g_warnedQuestMissing = false;

    RE::TESQuest* FindQuest()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler ? dataHandler->LookupForm<RE::TESQuest>(kUnboundQuest, "Skyrim.esm") : nullptr;
    }

    // Whether a stage has ever been run.
    bool HasRunStage(const RE::TESQuest& quest, std::uint16_t stage)
    {
        if (not quest.executedStages) return false;

        for (const auto& executed : *quest.executedStages)
        {
            if (executed.data.index == stage) return true;
        }
        return false;
    }

    // Returns whether the mod should be active, with the stage it is based on and the reason, for logging
    bool ShouldBeActive(std::uint16_t& outStage, std::string_view& outReason)
    {
        outStage = 0;

        auto* quest = FindQuest();
        if (not quest)
        {
            // Never lock the mod out because of a missing quest (e.g. a total conversion that drops the intro)
            if (not g_warnedQuestMissing)
            {
                g_warnedQuestMissing = true;
                SKSE::log::warn("[Activation] quest MQ101 (Skyrim.esm {:06X}) not found, the mod is always active", kUnboundQuest);
            }
            outReason = "quest not found";
            return true;
        }

        outStage = quest->GetCurrentStageID();

        if (quest->IsCompleted()) { outReason = "quest completed"; return true; }
        if (outStage >= kFreedStage) { outReason = "current stage is past the unbinding"; return true; }
        if (HasRunStage(*quest, kFreedStage)) { outReason = "the unbinding stage was run"; return true; }

        outReason = "player isn't unbound yet";
        return false;
    }

    // Sets up the state that depends on the loaded save, and switches on what changes the game (shout input)
    void InitializeActiveMod()
    {
        auto* self = RE::PlayerCharacter::GetSingleton();
        if (not self)
        {
            SKSE::log::error("[Activation] the mod became active but there is no player yet, its per-save setup was skipped");
            return;
        }

        SKSE::log::info("[Activation] setting up for this save: equipped items, restorative cache, combat tracking, shout suppression");

        ActorContext& selfCtx = GetActorContext(self);
        selfCtx.lastLeftHandLabel = GetHandSlotLabel(self, true);
        selfCtx.lastRightHandLabel = GetHandSlotLabel(self, false);
        selfCtx.prevLeftContent = GetHandContentName(self->GetEquippedObject(true));
        selfCtx.prevRightContent = GetHandContentName(self->GetEquippedObject(false));

        // Items picked up while the mod was dormant weren't tracked
        InitRestoratives(self);

        ResetCombatTracking();
        InitShoutSuppression();
    }

    // Gives the game back what the mod changed
    void ReleaseDormantMod()
    {
        ReleaseShoutSuppression();
        ResetCombatTracking();
    }

    // Whether the current save was seen dormant, i.e. the player is still in the Helgen intro and being unbound is still to come.
    // A save that is already past the intro just starts active, silently.
    bool g_awaitingUnbinding = false;

    void Apply(bool shouldBeActive, std::uint16_t stage, std::string_view reason, bool fromLoad)
    {
        // So the log always says which state the mod started in
        static bool loggedFirstCheck = false;
        if (not loggedFirstCheck)
        {
            loggedFirstCheck = true;
            SKSE::log::info("[Activation] first check: mod {} (MQ101 stage {}: {})", shouldBeActive ? "active" : "dormant", stage, reason);
        }

        if (fromLoad) g_awaitingUnbinding = not shouldBeActive;

        if (shouldBeActive == g_active and not (shouldBeActive and fromLoad)) return;

        const bool changed = shouldBeActive != g_active;
        g_active = shouldBeActive;

        if (g_active)
        {
            InitializeActiveMod();

            if (changed)
            {
                const bool announce = g_awaitingUnbinding and not fromLoad;
                SKSE::log::info("[Activation] mod active (MQ101 stage {}: {}){}", stage, reason,
                    fromLoad ? ", a save was loaded" : (announce ? ", the player was unbound, showing the message box" : ""));

                // Not when a save was just loaded: only when the unbinding happened while playing
                if (announce) RE::DebugMessageBox("TACTIC is now active.\nData collection has started.");
                g_awaitingUnbinding = false;
            }
        }
        else
        {
            SKSE::log::info("[Activation] mod dormant (MQ101 stage {}: {})", stage, reason);
            ReleaseDormantMod();
        }
    }

    // How often the state of the plugin is written to the log while the game runs
    constexpr std::chrono::seconds kStatusInterval{ 120 };
    auto g_lastStatus = std::chrono::steady_clock::now();
}

bool IsModActive()
{
    return g_active;
}

void TickModActivation(float dt)
{
    g_checkCooldown -= dt;
    if (g_checkCooldown > 0.0f) return;
    g_checkCooldown = kCheckIntervalSeconds;

    std::uint16_t stage = 0;
    std::string_view reason;
    const bool shouldBeActive = ShouldBeActive(stage, reason);
    Apply(shouldBeActive, stage, reason, false);

    // A line every couple of minutes. In a log sent after a crash, the last one shows what the plugin was doing around it
    if (std::chrono::steady_clock::now() - g_lastStatus >= kStatusInterval)
    {
        g_lastStatus = std::chrono::steady_clock::now();
        LogCollectionStatus(g_active);
    }
}

void NotifyGameLoaded()
{
    g_checkCooldown = kCheckIntervalSeconds;

    std::uint16_t stage = 0;
    std::string_view reason;
    const bool shouldBeActive = ShouldBeActive(stage, reason);
    Apply(shouldBeActive, stage, reason, true);
}
