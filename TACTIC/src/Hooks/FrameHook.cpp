#include "Hooks/FrameHook.h"

#include "DataCollection.h"
#include "Logging.h"
#include "Managers/CombatInputManager.h"
#include "Managers/ModActivationManager.h"
#include "Managers/ShoutSuppressionManager.h"
#include "Managers/UploadManager.h"

namespace 
{
    float& g_deltaTime = *reinterpret_cast<float*>(REL::VariantID(523660, 410199, 0x30C3A08).address());
}

/// <summary>
/// Install the FrameHook
/// </summary>
void FrameHook::Install() 
{
    static bool installed = false;
    if (installed) return;

    auto& trampoline = SKSE::GetTrampoline();
    const auto address = REL::VariantID(35565, 36564, 0x5BAB10).address();
    const auto frameUpdateOffset = REL::VariantOffset(0x748, 0xC26, 0x7EE).offset();

    // Logged before and after, so a crash while installing shows which hook it was
    SKSE::log::info("[Hooks] installing FrameHook at {}", Logging::GameOffset(address + frameUpdateOffset));
    _originalFrameUpdate = trampoline.write_call<5>(address + frameUpdateOffset, OnFrameUpdate);
    installed = true;
    SKSE::log::info("[Hooks] FrameHook installed");
}

/// <summary>
/// Execute functions that need to run of every frame
/// </summary>
void FrameHook::OnFrameUpdate()
{
    // Always first: switches the mod on once the Helgen intro is over, and nothing below runs until then
    TickModActivation(g_deltaTime);

    if (IsModActive())
    {
        TickSnapshotClock(g_deltaTime);
        TickCombatStateHistory();
        TickAttackStateTracking(g_deltaTime);
        TickResourceHistory();
        TickRestorativeCollection(g_deltaTime);
        TickCombatEnd(g_deltaTime);
        TickProjectileFiringCollection(g_deltaTime);
        TickTargetSwitchCollection();
        TickMovementCollection();
        TickHeartbeatCollection();
        TickShoutSuppression(g_deltaTime);
        TickPeriodicUpload(g_deltaTime);
    }

    _originalFrameUpdate();
}