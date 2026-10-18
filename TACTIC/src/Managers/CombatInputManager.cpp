#include "Managers/CombatInputManager.h"

#include "DataCollection.h"
#include "Managers/ActorContextManager.h"
#include "Managers/ModActivationManager.h"
#include "Managers/ActorStateManager.h"
#include "Managers/WeaponManager.h"

namespace {
    // How long after a release to keep checking for a newly-spawned player-shot projectile
    // before concluding the draw was cancelled rather than fired.
    constexpr float kProjectileFireCheckWindow = 0.3f;

    // A player-shot projectile younger than this when found during the check
    // window is treated as the one this release just fired, rather than one already in flight
    // from an earlier, still-traveling shot.
    constexpr float kMaxFreshProjectileAge = 0.3f;

    bool g_pendingProjectileCheck = false;
    float g_projectileCheckTimeRemaining = 0.0f;

    // How long after an attack-button press on a bow to wait for the draw to actually begin.
    constexpr float kDrawConfirmWindow = 0.6f;

    // The press and release snapshots are captured at the moment of the input but only written once the outcome is known: 
    // the press one once a draw is confirmed, the release one once it's known whether an arrow was fired or not.
    bool g_loadPending = false;       // press captured, waiting to see if a draw actually starts
    float g_loadConfirmTimeRemaining = 0.0f;
    bool g_drawCommitted = false;     // a LoadProjectile row was written for the current press
    std::vector<CombatSnapshotRow> g_loadRows;
    std::vector<CombatSnapshotRow> g_releaseRows;

    // Secondary signal to check for whether an arrow/bolt was actually fired
    bool g_pendingIsBolt = false;
    int g_ammoCountAtDrawStart = 0;
    bool g_drawStarted = false;

    // Held state for both attack/block buttons
    // Needed to tell a single-hand cast apart from a deliberate dual-cast.
    bool g_rightAttackHeld = false;
    bool g_leftAttackHeld = false;

    RE::ATTACK_STATE_ENUM g_lastAttackState = RE::ATTACK_STATE_ENUM::kNone;
    bool g_freshAttackWindupPending = false;

    bool g_wasBlocking = false;

    // A cast press is captured immediately but its label is only settled after a short chord window.
    // Used to label dual-casts.
    constexpr float kDualCastChordWindow = 0.1f;
    bool g_castPending = false;
    bool g_castPendingRight = false;
    float g_castPendingTimeRemaining = 0.0f;
    std::vector<CombatSnapshotRow> g_castRows;

    void CommitPendingCast(std::string_view label) 
    {
        if (!g_castPending) return;
        g_castPending = false;
        CommitDeferredRows(g_castRows, label);
    }

    // A melee/block button press is captured at the press, the moment of the decision, and only written once it resolves:
    // into a swing (labelled normal/power on release), a block or a bash. A press nothing comes of is discarded.
    constexpr float kMeleePressConfirmWindow = 0.6f;
    bool g_meleePressPending = false;
    bool g_meleePressRight = false;
    bool g_meleePressReleased = false;
    bool g_meleePressWindupSeen = false;
    float g_meleePressTimeRemaining = 0.0f;
    std::vector<CombatSnapshotRow> g_meleePressRows;

    void ResolveMeleePress(RE::PlayerCharacter* self, std::string_view label)
    {
        g_meleePressPending = false;

        // Nothing was collectible at the press (e.g. the press itself started the combat), so fall back to the live state
        if (g_meleePressRows.empty()) CollectData(self, label);
        else CommitDeferredRows(g_meleePressRows, label);
    }

    void DiscardMeleePress()
    {
        g_meleePressPending = false;
        g_meleePressRows.clear();
    }

    // Check for dual normal or power attacks
    std::string_view GetMeleeAttackLabel(bool isRightHand, bool isPowerAttack)
    {
        if (isRightHand ? g_leftAttackHeld : g_rightAttackHeld) return isPowerAttack ? "BothPowerAttack" : "BothNormalAttack";
        if (isRightHand) return isPowerAttack ? "RightPowerAttack" : "RightNormalAttack";
        return isPowerAttack ? "LeftPowerAttack" : "LeftNormalAttack";
    }

    // Which hand the player's most recent favorites-menu equip (RightEquip/LeftEquip) targeted
    bool g_hasRecentMenuEquipSide = false;
    bool g_recentMenuEquipWasLeft = false;

    // Consider IsBlocking to be true only when there is no active attack phase
    bool IsBlockingNow(RE::PlayerCharacter* self) 
    {
        AttackPhase phase = GetAttackPhase(self);
        return self->IsBlocking() and phase != AttackPhase::kWindup and phase != AttackPhase::kRelease;
    }

    bool HasFreshSelfProjectile(RE::PlayerCharacter* self, const RE::BSTArray<RE::ProjectileHandle>& handles) 
    {
        for (auto& handle : handles) 
        {
            RE::Projectile* projectile = handle.get().get();
            if (!projectile) continue;

            auto* shooterRef = projectile->GetProjectileRuntimeData().shooter.get().get();
            if (!shooterRef || shooterRef->As<RE::Actor>() != self) continue;

            if (projectile->GetProjectileRuntimeData().livingTime <= kMaxFreshProjectileAge) return true;
        }
        return false;
    }

    // The player's live attack data at the moment of release.
    RE::BGSAttackData* GetLiveAttackData(RE::PlayerCharacter* self) 
    {
        auto* process = self->GetActorRuntimeData().currentProcess;
        auto* high = process ? process->high : nullptr;
        return high ? high->attackData.get() : nullptr;
    }

    bool IsLivePowerAttack(RE::PlayerCharacter* self)
    {
        auto* attackData = GetLiveAttackData(self);
        return attackData and attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
    }

    // Sinks the player's attack/cast/block input.
    class CombatInputHandler : public RE::BSTEventSink<RE::InputEvent*> 
    {
    public:
        static CombatInputHandler* GetSingleton() 
        {
            static CombatInputHandler s;
            return &s;
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override 
        {
            if (!events) return RE::BSEventNotifyControl::kContinue;
            if (not IsModActive()) return RE::BSEventNotifyControl::kContinue;

            auto* self = RE::PlayerCharacter::GetSingleton();
            if (!self) return RE::BSEventNotifyControl::kContinue;

            void* lastProcessedAttackData = nullptr;

            for (auto* event = *events; event; event = event->next) 
            {
                auto* button = event->AsButtonEvent();
                if (!button) continue;
                if (!button->IsDown() && !button->IsUp()) continue;

                std::string_view userEvent = button->GetUserEvent();

                if (button->IsDown() && (userEvent == "RightEquip" || userEvent == "LeftEquip")) 
                {
                    g_hasRecentMenuEquipSide = true;
                    g_recentMenuEquipWasLeft = userEvent == "LeftEquip";
                }

                bool isRightHandInput = userEvent == "Right Attack/Block";
                bool isLeftHandInput = userEvent == "Left Attack/Block";
                if (!isRightHandInput && !isLeftHandInput) continue;

                RE::TESForm* equipped = self->GetEquippedObject(isLeftHandInput);
                int handType = GetHandType(equipped);

                // Bow/crossbow: only ever occupies the right-hand slot.
                if (isRightHandInput && (handType == 7 || handType == 9))
                {
                    bool isCrossbow = handType == 9;
                    if (button->IsDown()) 
                    {
                        g_pendingIsBolt = isCrossbow;
                        g_ammoCountAtDrawStart = CountAmmo(self, g_pendingIsBolt);
                        g_drawStarted = true;
                        g_drawCommitted = false;

                        // The draw has already started by now, so the attack/aim state comes from the previous frame
                        const SelfCombatStateSample prePressState = GetActorContext(self).combatSamplePrev;
                        CollectData(self, "LoadProjectile", false, false, false, nullptr, &g_loadRows, nullptr, &prePressState);
                        g_loadPending = true;
                        g_loadConfirmTimeRemaining = kDrawConfirmWindow;
                    } 
                    else if (g_drawStarted) 
                    {
                        if (g_loadPending) 
                        {
                            // Released before any draw ever began — nothing happened in-game.
                            g_loadRows.clear();
                            g_loadPending = false;
                            g_drawStarted = false;
                        } else if (g_drawCommitted) 
                        {
                            // The release has already started by now, so the state is the previous frame's: the draw still held and aimed
                            const SelfCombatStateSample preReleaseState = GetActorContext(self).combatSamplePrev;
                            CollectData(self, "FireProjectile", false, false, false, nullptr, &g_releaseRows, nullptr, &preReleaseState);
                            g_pendingProjectileCheck = true;
                            g_projectileCheckTimeRemaining = kProjectileFireCheckWindow;
                        } 
                        else g_drawStarted = false;
                    }
                    continue;
                }

                if (isRightHandInput) g_rightAttackHeld = button->IsDown();
                else g_leftAttackHeld = button->IsDown();
                
                bool isCastable = (equipped and equipped->As<RE::SpellItem>()) or handType == 8;

                if (button->IsDown()) 
                {
                    if (isCastable) 
                    {
                        if (g_castPending && g_castPendingRight != isRightHandInput) 
                        {
                            CommitPendingCast("CastBoth");
                            continue;
                        }
                        CommitPendingCast(g_castPendingRight ? "CastRight" : "CastLeft");

                        // Other hand already held from earlier
                        if (isRightHandInput ? g_leftAttackHeld : g_rightAttackHeld) CollectData(self, "CastBoth");
                        else 
                        {
                            CollectData(self, isRightHandInput ? "CastRight" : "CastLeft", false, false, false, nullptr, &g_castRows);
                            g_castPending = true;
                            g_castPendingRight = isRightHandInput;
                            g_castPendingTimeRemaining = kDualCastChordWindow;
                        }
                    }
                    // A second press while one is pending (a dual-wield chord) keeps the first press as the decision
                    else if (not g_meleePressPending)
                    {
                        // The game has already started the swing/block by now, so the attack/block state comes from the previous frame
                        const SelfCombatStateSample prePressState = GetActorContext(self).combatSamplePrev;
                        CollectData(self, isRightHandInput ? "RightNormalAttack" : "LeftNormalAttack", false, false, false, nullptr, &g_meleePressRows, nullptr, &prePressState);
                        g_meleePressPending = true;
                        g_meleePressRight = isRightHandInput;
                        g_meleePressReleased = false;
                        g_meleePressWindupSeen = false;
                        g_meleePressTimeRemaining = kMeleePressConfirmWindow;
                    }
                    continue;
                }

                if (isCastable) continue;  // Cast already fired on press

                // Letting go of the block button while blocking is the decision to drop the guard.
                // The game has already processed the release by now, so the state is the previous frame's: still blocking
                if (isLeftHandInput and GetActorContext(self).combatSamplePrev.isBlocking)
                {
                    const SelfCombatStateSample preReleaseState = GetActorContext(self).combatSamplePrev;
                    CollectData(self, "BlockRelease", false, false, false, nullptr, nullptr, nullptr, &preReleaseState);
                    continue;
                }

                if (g_meleePressPending)
                {
                    g_meleePressReleased = true;

                    // The swing already began: normal vs. power is only known now.
                    // Otherwise TickAttackStateTracking labels it once the swing, block or bash begins
                    if (g_meleePressWindupSeen)
                    {
                        g_freshAttackWindupPending = false;
                        ResolveMeleePress(self, GetMeleeAttackLabel(isRightHandInput, IsLivePowerAttack(self)));
                    }
                    continue;
                }

                // No press was captured for this swing, so the release-time state is the closest available
                if (auto* attackData = GetLiveAttackData(self))
                {
                    if (attackData == lastProcessedAttackData) continue;
                    lastProcessedAttackData = attackData;

                    std::string_view animEventName{attackData->event.c_str()};
                    bool isPowerAttack = attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);

                    if (animEventName.starts_with("bash")) continue;

                    if (g_freshAttackWindupPending)
                    {
                        // Only collect once per actual new swing
                        g_freshAttackWindupPending = false;
                        CollectData(self, GetMeleeAttackLabel(isRightHandInput, isPowerAttack));
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void RegisterCombatInputHandler() 
{
    RE::BSInputDeviceManager::GetSingleton()->AddEventSink(CombatInputHandler::GetSingleton());
}

bool ConsumeRecentMenuEquipSide(bool& outWasLeftHand)
{
    if (!g_hasRecentMenuEquipSide) return false;

    outWasLeftHand = g_recentMenuEquipWasLeft;
    g_hasRecentMenuEquipSide = false;
    return true;
}

bool HasPendingDeferredRows()
{
    return not g_loadRows.empty() or not g_releaseRows.empty() or not g_castRows.empty() or g_meleePressPending;
}

void TickAttackStateTracking(float dt)
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (!self) return;

    auto state = self->AsActorState()->GetAttackState();

    if (state == RE::ATTACK_STATE_ENUM::kDraw && g_lastAttackState != RE::ATTACK_STATE_ENUM::kDraw)
    {
        g_freshAttackWindupPending = true;
        if (g_meleePressPending) g_meleePressWindupSeen = true;
    }

    if (state == RE::ATTACK_STATE_ENUM::kBash && g_lastAttackState != RE::ATTACK_STATE_ENUM::kBash)
    {
        if (g_meleePressPending) ResolveMeleePress(self, "Bash");
        else CollectData(self, "Bash");
    }
    g_lastAttackState = state;

    // A tap released before its swing began is labelled as soon as the swing does
    if (g_meleePressPending and g_meleePressWindupSeen and g_meleePressReleased)
    {
        g_freshAttackWindupPending = false;
        ResolveMeleePress(self, GetMeleeAttackLabel(g_meleePressRight, IsLivePowerAttack(self)));
    }

    bool isBlocking = IsBlockingNow(self);
    if (isBlocking && not g_wasBlocking)
    {
        if (g_meleePressPending and not g_meleePressWindupSeen) ResolveMeleePress(self, "Block");
        else CollectData(self, "Block");
    }
    g_wasBlocking = isBlocking;

    // Nothing came of the press (e.g. no stamina, staggered)
    if (g_meleePressPending and not g_meleePressWindupSeen)
    {
        g_meleePressTimeRemaining -= dt;
        if (g_meleePressTimeRemaining <= 0.0f) DiscardMeleePress();
    }
}

void TickProjectileFiringCollection(float dt) 
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (!self) return;

    // Dual-cast chord window expired without the other hand's press.
    if (g_castPending) 
    {
        g_castPendingTimeRemaining -= dt;
        if (g_castPendingTimeRemaining <= 0.0f) CommitPendingCast(g_castPendingRight ? "CastRight" : "CastLeft");
    }

    // Waiting to see whether the press actually started a draw.
    if (g_loadPending)
    {
        if (IsAimingRangedWeapon(self)) 
        {
            CommitDeferredRows(g_loadRows, "LoadProjectile");
            g_loadPending = false;
            g_drawCommitted = true;
        } 
        else 
        {
            g_loadConfirmTimeRemaining -= dt;
            if (g_loadConfirmTimeRemaining <= 0.0f) 
            {
                g_loadRows.clear();
                g_loadPending = false;
                g_drawStarted = false;
            }
        }
    }

    if (!g_pendingProjectileCheck) return;

    auto* manager = RE::Projectile::Manager::GetSingleton();
    bool projectileSeen = manager && (HasFreshSelfProjectile(self, manager->unlimited) || HasFreshSelfProjectile(self, manager->limited));
    bool ammoConsumed = CountAmmo(self, g_pendingIsBolt) < g_ammoCountAtDrawStart;

    if (projectileSeen || ammoConsumed) {
        g_pendingProjectileCheck = false;
        g_drawStarted = false;
        g_drawCommitted = false;

        CommitDeferredRows(g_releaseRows, "FireProjectile");
        return;
    }

    g_projectileCheckTimeRemaining -= dt;
    if (g_projectileCheckTimeRemaining <= 0.0f) 
    {
        g_pendingProjectileCheck = false;  // draw was cancelled, nothing fired
        g_drawStarted = false;
        g_drawCommitted = false;

        CommitDeferredRows(g_releaseRows, "CancelProjectile");
    }
}
