#include "DataCollection.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#include <limits>
#include <map>
#include <numeric>
#include <unordered_set>

#include "Hooks/EnemyMarkerHook.h"
#include "Hooks/ParalysisBlockHook.h"

#include "Managers/ActorContextManager.h"
#include "Managers/ActorStateManager.h"
#include "Managers/AngleManager.h"
#include "Managers/ArmorManager.h"
#include "Managers/CombatInputManager.h"
#include "Managers/DistanceManager.h"
#include "Managers/EnemyStrengthManager.h"
#include "Managers/GeometryManager.h"
#include "Managers/ModActivationManager.h"
#include "Managers/HumanoidRacesManager.h"
#include "Managers/PerkManager.h"
#include "Managers/ProjectileManager.h"
#include "Managers/ResourceManager.h"
#include "Managers/ShoutSuppressionManager.h"
#include "Managers/SkillManager.h"
#include "Managers/WeaponManager.h"

#include "CSVWriter.h"
#include "Utils.h"


// Generate a random 64-bit integer for this session so as to differentiate every different game launch instance
static uint64_t GenerateSessionID()
{
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
}

// SessionID never changes until the game closes and relaunches
static const uint64_t g_sessionID = GenerateSessionID();

uint64_t GetSessionID() { return g_sessionID; }

// Identifier for the current combat
static uint64_t g_combatID = 0;

// Start collecting for a new combat
static bool g_startNewCombat = true;

// Defines whether the player is in combat or not
static bool g_inCombat = false;

bool IsCollectingCombat() { return g_inCombat; }

// Player fully-detected
static bool g_wasFullyDetected = false;

// If the list of red dots empties, start a new combat
constexpr float kCombatEndEmptySeconds = 2.0f;  // how long the dot list must stay empty
static float g_secondsWithoutMarkers = 0.0f;
static bool g_markersSeenThisCombat = false;

// Whether a pausing menu (inventory, lockpicking, settings, ...) is open
static bool IsGamePaused()
{
    auto* ui = RE::UI::GetSingleton();
    return ui and ui->GameIsPaused();
}

// What happened since the player entered the current combat, written to the log as a single line when it ends
struct CombatLogStats
{
    uint64_t snapshots = 0;
    uint64_t rows = 0;
    uint64_t firstSnapshotMs = 0;
    uint64_t lastSnapshotMs = 0;
    uint64_t combatID = 0;

    // Heartbeats that wrote nothing, by the reason the collection gave
    std::map<std::string, int> skippedHeartbeats;
};
static CombatLogStats g_combatLog;

// Totals since the game was launched, for the status line
static uint64_t g_sessionSnapshots = 0;
static uint64_t g_sessionRows = 0;

// "Reason=count, ..." for the heartbeats that wrote nothing since the current combat began
static std::string DescribeSkippedHeartbeats()
{
    std::string skipped;
    for (const auto& [why, count] : g_combatLog.skippedHeartbeats)
    {
        if (not skipped.empty()) skipped += ", ";
        skipped += std::format("{}={}", why, count);
    }
    return skipped;
}

static void LogCombatEnd(std::string_view reason)
{
    if (g_combatLog.snapshots == 0 and g_combatLog.skippedHeartbeats.empty())
    {
        g_combatLog = {};
        return;
    }

    SKSE::log::info("[Combat] ended ({}): CombatID={} snapshots={} rows={} lengthMs={} skippedHeartbeats=[{}]",
        reason, g_combatLog.snapshots > 0 ? std::to_string(g_combatLog.combatID) : "none", g_combatLog.snapshots, g_combatLog.rows,
        g_combatLog.lastSnapshotMs - g_combatLog.firstSnapshotMs, DescribeSkippedHeartbeats());

    g_combatLog = {};
}

void TickCombatEnd(float dt)
{
    // Time spent in a menu doesn't count toward the empty-dot-list timer   
    if (IsGamePaused()) return;

    if (EnemyMarkerHook::AnyEnemyMarkerShown())
    {
        g_secondsWithoutMarkers = 0.0f;
        g_markersSeenThisCombat = true;
        return;
    }

    // Only a combat that actually had dots can end this way, and only one that is open.
    if (not g_inCombat or not g_markersSeenThisCombat) return;

    g_secondsWithoutMarkers += dt;
    if (g_secondsWithoutMarkers < kCombatEndEmptySeconds) return;

    LogCombatEnd("no enemy markers");

    g_inCombat = false;
    g_markersSeenThisCombat = false;
    g_secondsWithoutMarkers = 0.0f;
    ResetCombatState();
}

void ResetCombatTracking()
{
    LogCombatEnd("tracking reset");

    g_inCombat = false;
    g_markersSeenThisCombat = false;
    g_secondsWithoutMarkers = 0.0f;
    ResetCombatState();
}

// Identifier for the entire session
static uint64_t g_snapshotID = 0;

// Unpaused game time since the session started
static double g_gameTimeSeconds = 0.0;

// Time since the last snapshot was captured. Any snapshot restarts it
static float g_secondsSinceLastSnapshot = 0.0f;

static uint64_t GetGameTimeMs() { return static_cast<uint64_t>(g_gameTimeSeconds * 1000.0); }

void TickSnapshotClock(float dt)
{
    if (IsGamePaused()) return;
    g_gameTimeSeconds += dt;

    // Advanced at the start of the frame, so a snapshot captured later this frame restarts it from 0
    // and the next heartbeat is a full interval later
    g_secondsSinceLastSnapshot += dt;
}

void LogCollectionStatus(bool modActive)
{
    // The skip counts are for the combat that is still open. A combat the game keeps flagging while no enemy can see the player can stay open for minutes
    // and only gets its summary when it ends, so without them the log wouldn't say why nothing is being written
    SKSE::log::info("[Status] mod={} inCombat={} combatID={} sessionSnapshots={} sessionRows={} csvOpen={} paralysisBlocked={} openCombatSkips=[{}]",
        modActive ? "active" : "dormant", g_inCombat, g_combatID, g_sessionSnapshots, g_sessionRows, CSVWriter::GetSingleton()->IsOpen(),
        ParalysisBlockHook::GetBlockedCount(), DescribeSkippedHeartbeats());
}

// The actionType passed to CollectData by heartbeats. Not a combat or inventory action, so both labels become "None"
constexpr std::string_view kHeartbeatActionType = "Heartbeat";

// Writes one snapshot's rows
static void WriteSnapshotRows(std::vector<CombatSnapshotRow>& rows)
{
    if (rows.empty()) return;

    // First row of a new combat
    if (g_startNewCombat)
    {
        g_startNewCombat = false;
        g_combatID++;
        g_snapshotID = 0;
        SKSE::log::info("[Combat] started: CombatID={} ({} enemies in this snapshot's rows)", g_combatID, rows.size());
    }

    g_snapshotID++;

    if (g_combatLog.snapshots == 0) g_combatLog.firstSnapshotMs = GetGameTimeMs();
    g_combatLog.combatID = g_combatID;
    g_combatLog.snapshots++;
    g_combatLog.rows += rows.size();
    g_combatLog.lastSnapshotMs = GetGameTimeMs();
    g_sessionSnapshots++;
    g_sessionRows += rows.size();

    // One timestamp per snapshot
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
    localtime_s(&localTime, &now);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &localTime);

    for (auto& row : rows)
    {
        row.combatID = g_combatID;
        row.snapshotID = g_snapshotID;
        row.timestamp = timestamp;
        CSVWriter::GetSingleton()->WriteRow(row);
    }
}
// Quiet period after the last consumption in a live burst (hotkey use, game not paused).
constexpr float kRestorativeCollectionDebounce = 1.0f;

// One sample of the player's resource state, from the live values
static SelfResourceSample SampleResources(RE::PlayerCharacter* self)
{
    SelfResourceSample sample;
    sample.healthLevel = CalculateResourceLevel(self, RE::ActorValue::kHealth);
    sample.staminaLevel = CalculateResourceLevel(self, RE::ActorValue::kStamina);
    sample.magickaLevel = CalculateResourceLevel(self, RE::ActorValue::kMagicka);
    sample.hasHealthRestorative = HasRestorative(self, RestoreType::kHealth);
    sample.hasStaminaRestorative = HasRestorative(self, RestoreType::kStamina);
    sample.hasMagickaRestorative = HasRestorative(self, RestoreType::kMagicka);
    return sample;
}

void TickResourceHistory()
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    ActorContext& ctx = GetActorContext(self);
    ctx.samplePrev = ctx.sampleNow;
    ctx.sampleNow = SampleResources(self);
}

// One sample of the player's attack/block state, from the live values
static SelfCombatStateSample SampleCombatState(RE::PlayerCharacter* self)
{
    SelfCombatStateSample sample;
    sample.attackPhase = GetAttackPhase(self);

    // A swing that's winding up or releasing can't coexist with blocking
    const bool isStartingAttack = sample.attackPhase == AttackPhase::kWindup or sample.attackPhase == AttackPhase::kRelease;
    sample.isBlocking = self->IsBlocking() and not isStartingAttack;
    sample.isAimingRanged = IsAimingRangedWeapon(self);
    return sample;
}

void TickCombatStateHistory()
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    ActorContext& ctx = GetActorContext(self);
    ctx.combatSamplePrev = ctx.combatSampleNow;
    ctx.combatSampleNow = SampleCombatState(self);
}

// At most this many enemies get rows per snapshot (the model's enemy slots). EnemyCount still counts all of them
constexpr std::size_t kMaxTrackedEnemies = 8;

// The enemies that got rows in the previous snapshot of this combat, so the same ones keep their rows instead of rotating
static std::unordered_set<RE::FormID> g_trackedEnemies;

// The enemy picked as the movement frame when the player has no HUD target, kept while it stays visible so the frame doesn't flip between snapshots
static RE::FormID g_fallbackTargetFormID = 0;

// The enemy the movement frame ("forward", the clearance and drop rays) points at: the player's HUD target or, without one, the nearest visible enemy.
// The NPC always has a combat target, so the player's frame always needs one too. A fallback is only a reference direction, not something the player chose:
// the rows say so in TargetSource, and its health and strength stay unknown because only the HUD shows those.
// Returns 0 when there's neither a HUD target nor a visible enemy.
static RE::FormID ChooseFrameTarget(const std::vector<CombatSnapshotRow>& rows, const std::vector<float>& distances, std::string_view& outSource)
{
    for (const auto& row : rows)
    {
        if (not row.enemyIsSelfTarget) continue;

        g_fallbackTargetFormID = 0;
        outSource = "Hud";
        return row.enemyFormID;
    }

    outSource = "NearestVisible";

    // Keep the previous fallback while it's still among the visible enemies
    for (const auto& row : rows)
    {
        if (row.enemyFormID == g_fallbackTargetFormID and row.enemyIsVisible) return g_fallbackTargetFormID;
    }

    g_fallbackTargetFormID = 0;
    float nearest = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < rows.size(); i++)
    {
        if (not rows[i].enemyIsVisible or distances[i] >= nearest) continue;

        nearest = distances[i];
        g_fallbackTargetFormID = rows[i].enemyFormID;
    }
    return g_fallbackTargetFormID;
}

// Picks which enemies get rows: the frame target first, then visible enemies, then the rest,
// preferring enemies already tracked so the set stays stable. Updates g_trackedEnemies to the chosen ones.
static void KeepTrackedEnemies(std::vector<CombatSnapshotRow>& rows, const std::vector<float>& distances, RE::FormID frameTargetFormID)
{
    // 0: frame target, 1: visible and tracked, 2: visible, 3: tracked, 4: the rest
    auto tier = [frameTargetFormID](const CombatSnapshotRow& row)
    {
        const bool tracked = g_trackedEnemies.contains(row.enemyFormID);
        if (row.enemyIsSelfTarget or row.enemyFormID == frameTargetFormID) return 0;
        if (row.enemyIsVisible) return tracked ? 1 : 2;
        return tracked ? 3 : 4;
    };

    std::vector<std::size_t> order(rows.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b)
    {
        const int tierA = tier(rows[a]);
        const int tierB = tier(rows[b]);
        if (tierA != tierB) return tierA < tierB;

        // Nearest first among visible enemies. An unseen enemy's distance isn't known to the player, so those just keep a stable order
        if (tierA <= 2) return distances[a] < distances[b];
        return rows[a].enemyFormID < rows[b].enemyFormID;
    });
    if (order.size() > kMaxTrackedEnemies) order.resize(kMaxTrackedEnemies);

    // Enemies that died, despawned or left the fight drop out here, since they're no longer among the rows
    std::vector<CombatSnapshotRow> kept;
    g_trackedEnemies.clear();
    for (std::size_t index : order)
    {
        g_trackedEnemies.insert(rows[index].enemyFormID);
        kept.push_back(std::move(rows[index]));
    }
    rows = std::move(kept);
}

// How far (about 1 m) an NPC's feet can be above or below the player's and still count as the same level: roughly where melee stops reaching
constexpr float kSameLevelHeightTolerance = 70.0f;

// Half-width (degrees) of the compass's visible-marker window (approximate)
constexpr float kCompassHalfWidthDegrees = 50.0f;

// How far above an actor's own "SpeedRun" animation-graph reference its live "Speed" needs to be
// to count as sprinting rather than just running.
constexpr float kSprintSpeedRatio = 1.1f;

void NotifyRestorativeConsumed(RE::Actor* actor, RestoreType type)
{
    if (not actor) return;

    ActorContext& ctx = GetActorContext(actor);

    if (type == RestoreType::kNone) return;

    // First item of a new burst
    const bool burstAlreadyPending = ctx.pendingHealthRestorative or ctx.pendingStaminaRestorative or ctx.pendingMagickaRestorative;
    if (not burstAlreadyPending)
    {
        ctx.burstPreState = ctx.samplePrev;
        ctx.burstInMenu = false;
    }

    switch (type)
    {
        case RestoreType::kHealth:
            ctx.pendingHealthRestorative = true;
            break;
        case RestoreType::kStamina:
            ctx.pendingStaminaRestorative = true;
            break;
        case RestoreType::kMagicka:
            ctx.pendingMagickaRestorative = true;
            break;
        default:
            return;
    }

    ctx.restorativeCollectionCooldown = kRestorativeCollectionDebounce;
}

void TickRestorativeCollection(float dt)
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    ActorContext& ctx = GetActorContext(self);

    if (not ctx.pendingHealthRestorative and not ctx.pendingStaminaRestorative and not ctx.pendingMagickaRestorative) return;

    // A burst inside a menu (inventory/favorites — the game is paused there) is held until the game unpauses
    auto* ui = RE::UI::GetSingleton();
    if (ui and ui->GameIsPaused())
    {
        ctx.burstInMenu = true;
        return;
    }

    if (not ctx.burstInMenu)
    {
        ctx.restorativeCollectionCooldown -= dt;
        if (ctx.restorativeCollectionCooldown > 0.0f) return;
    }

    bool consumedHealth = ctx.pendingHealthRestorative;
    bool consumedStamina = ctx.pendingStaminaRestorative;
    bool consumedMagicka = ctx.pendingMagickaRestorative;

    ctx.pendingHealthRestorative = false;
    ctx.pendingStaminaRestorative = false;
    ctx.pendingMagickaRestorative = false;
    ctx.burstInMenu = false;

    CollectData(self, "ConsumeRestorative", consumedHealth, consumedStamina, consumedMagicka, &ctx.burstPreState);
}

void NotifyHandsChanged(RE::Actor* actor)
{
    if (not actor) return;

    ActorContext& ctx = GetActorContext(actor);

    // A swap is several equip events in a row
    constexpr double kEquipBurstGapSeconds = 1.0;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - ctx.lastEquipEventTime > kEquipBurstGapSeconds)
    {
        ctx.burstPreLeftContent = ctx.prevLeftContent;
        ctx.burstPreRightContent = ctx.prevRightContent;
    }
    ctx.lastEquipEventTime = now;

    if (ctx.lastLeftHandLabel.ends_with("Both") and ctx.lastLeftHandLabel == ctx.lastRightHandLabel)
        ctx.inTwoHandedBreakup = true;

    bool leftChanged = false;
    bool rightChanged = false;

    std::string currentLeft = GetHandSlotLabel(actor, true);
    if (not currentLeft.empty() and currentLeft != ctx.lastLeftHandLabel)
    {
        ctx.lastLeftHandLabel = std::move(currentLeft);
        leftChanged = true;
    }

    std::string currentRight = GetHandSlotLabel(actor, false);
    if (not currentRight.empty() and currentRight != ctx.lastRightHandLabel)
    {
        ctx.lastRightHandLabel = std::move(currentRight);
        rightChanged = true;
    }

    if (ctx.inTwoHandedBreakup)
    {
        bool leftIsUnarmed = ctx.lastLeftHandLabel == "EquipUnarmedLeft";
        bool rightIsUnarmed = ctx.lastRightHandLabel == "EquipUnarmedRight";

        // Never collect the transient "went unarmed mid-swap" step itself.
        if (leftChanged and leftIsUnarmed) leftChanged = false;
        if (rightChanged and rightIsUnarmed) rightChanged = false;

        if ((leftChanged and not leftIsUnarmed) or (rightChanged and not rightIsUnarmed))
        {
            bool menuEquipWasLeft = false;
            if (ConsumeRecentMenuEquipSide(menuEquipWasLeft))
            {
                if (menuEquipWasLeft) ctx.suppressNextRightHandChange = true;
                else ctx.suppressNextLeftHandChange = true;
            }
            else if (leftChanged != rightChanged)
            {
                if (leftChanged) ctx.suppressNextRightHandChange = true;
                else ctx.suppressNextLeftHandChange = true;
            }
        }

        if (not leftIsUnarmed and not rightIsUnarmed) ctx.inTwoHandedBreakup = false;
    }

    if (ctx.suppressNextLeftHandChange and leftChanged)
    {
        leftChanged = false;
        ctx.suppressNextLeftHandChange = false;
    }

    if (ctx.suppressNextRightHandChange and rightChanged)
    {
        rightChanged = false;
        ctx.suppressNextRightHandChange = false;
    }

    if (not ctx.inTwoHandedBreakup)
    {
        ctx.suppressNextLeftHandChange = false;
        ctx.suppressNextRightHandChange = false;
    }

    // Each hand's change is its own action
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (self and actor == self and (leftChanged or rightChanged))
    {
        SelfHandContents preHands;
        preHands.left = ctx.burstPreLeftContent;
        preHands.right = ctx.burstPreRightContent;

        auto isCollectible = [](const std::string& label) { return not label.starts_with("EquipUnarmed"); };

        if (leftChanged and rightChanged and ctx.lastLeftHandLabel == ctx.lastRightHandLabel)
        {
            // One two-handed item (or bow) landing in both hands at once is a single action.
            if (isCollectible(ctx.lastLeftHandLabel))
                CollectData(self, ctx.lastLeftHandLabel, false, false, false, nullptr, nullptr, &preHands);
        }
        else
        {
            if (leftChanged and isCollectible(ctx.lastLeftHandLabel))
                CollectData(self, ctx.lastLeftHandLabel, false, false, false, nullptr, nullptr, &preHands);
            if (rightChanged and isCollectible(ctx.lastRightHandLabel))
                CollectData(self, ctx.lastRightHandLabel, false, false, false, nullptr, nullptr, &preHands);
        }
    }

    ctx.prevLeftContent = GetHandContentName(actor->GetEquippedObject(true));
    ctx.prevRightContent = GetHandContentName(actor->GetEquippedObject(false));
}

std::string_view GetMovementTypeLabel(RE::PlayerCharacter* self);

// True for an actor that's alive, or bleeding out
static bool IsAliveOrBleedingOut(RE::Actor* actor)
{
    if (actor->AsActorState()->IsBleedingOut()) return true;

    return actor->GetLifeState() == RE::ACTOR_LIFE_STATE::kAlive
        and actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth) > 0.0f;
}

void TickTargetSwitchCollection()
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    // Skipped while paused so a target change seen in a menu isn't consumed without a row
    if (IsGamePaused()) return;

    auto crosshairTargetHandle = RE::CrosshairPickData::GetSingleton()->GetActiveTarget();
    RE::TESObjectREFR* crosshairTargetRef = crosshairTargetHandle ? crosshairTargetHandle.get().get() : nullptr;
    RE::Actor* crosshairTarget = crosshairTargetRef ? crosshairTargetRef->As<RE::Actor>() : nullptr;

    if (not crosshairTarget) return;

    if (not IsAliveOrBleedingOut(crosshairTarget)) return;
    if (not crosshairTarget->IsHostileToActor(self) and not self->IsHostileToActor(crosshairTarget)) return;

    ActorContext& ctx = GetActorContext(self);
    RE::FormID currentTarget = crosshairTarget->GetFormID();

    if (currentTarget == ctx.lastCrosshairTarget) return;

    // A zero lastCrosshairTarget means there was no genuine prior target to switch away from
    bool isGenuineSwitch = ctx.lastCrosshairTarget != 0;

    ctx.lastCrosshairTarget = currentTarget;
    CollectData(self, isGenuineSwitch ? "TargetSwitch" : GetMovementTypeLabel(self));
}

constexpr float kMovementInputDeadzone = 0.1f;

constexpr float kWorldHeadingChangeThresholdDeg = 20.0f;

// Signed angular difference (a - b) in degrees, wrapped to (-180, 180].
float AngleDeltaDeg(float a, float b)
{
    float diff = std::fmod(a - b, 360.0f);
    if (diff <= -180.0f) diff += 360.0f;
    if (diff > 180.0f) diff -= 360.0f;
    return diff;
}

// The 8-way direction of an angle in degrees, clockwise from forward
static std::string_view DirectionFromAngle(float angleDeg)
{
    angleDeg = AngleDeltaDeg(angleDeg, 0.0f);

    if (angleDeg >= -22.5f and angleDeg < 22.5f) return "forward";
    if (angleDeg >= 22.5f and angleDeg < 67.5f) return "forward_right";
    if (angleDeg >= 67.5f and angleDeg < 112.5f) return "right";
    if (angleDeg >= 112.5f and angleDeg < 157.5f) return "back_right";
    if (angleDeg >= 157.5f or angleDeg < -157.5f) return "back";
    if (angleDeg >= -157.5f and angleDeg < -112.5f) return "back_left";
    if (angleDeg >= -112.5f and angleDeg < -67.5f) return "left";
    return "forward_left";
}

// The angle of the player's WASD input in degrees, clockwise from the camera's forward. False with no input
static bool GetMovementInputAngle(float& outAngleDeg)
{
    auto* controls = RE::PlayerControls::GetSingleton();
    if (not controls) return false;

    const RE::NiPoint2& moveVec = controls->data.moveInputVec;
    if (moveVec.SqrLength() < kMovementInputDeadzone * kMovementInputDeadzone) return false;

    outAngleDeg = std::atan2(moveVec.x, moveVec.y) * (180.0f / std::numbers::pi_v<float>);
    return true;
}

// The player's raw WASD input direction, relative to the camera.
std::string_view GetMovementDirectionLabel(RE::PlayerCharacter*)
{
    float inputAngleDeg = 0.0f;
    if (not GetMovementInputAngle(inputAngleDeg)) return "None";

    return DirectionFromAngle(inputAngleDeg);
}

// The direction the player is moving relative to their target, "forward" meaning toward it: the frame the NPC's movement uses.
// Without a target, relative to the player's heading.
static std::string_view GetMovementDirectionToTargetLabel(RE::PlayerCharacter* self, RE::Actor* target)
{
    float inputAngleDeg = 0.0f;
    if (not GetMovementInputAngle(inputAngleDeg)) return "None";

    // WASD moves relative to the camera, so both angles are taken from the camera and the camera itself cancels out
    RE::NiPoint3 referencePoint;
    if (target and target != self) referencePoint = target->GetPosition();
    else
    {
        const float heading = self->GetAngleZ();
        referencePoint = self->GetPosition() + RE::NiPoint3{ std::sin(heading), std::cos(heading), 0.0f } * 100.0f;
    }
    const float referenceAngleDeg = GetCameraRelativeAngle(referencePoint);

    return DirectionFromAngle(AngleDeltaDeg(inputAngleDeg, referenceAngleDeg));
}

// A single-value summary of the player's current movement state
std::string_view GetMovementTypeLabel(RE::PlayerCharacter* self, std::string_view direction)
{
    if (self->IsInJumpState()) return "Jump";
    if (direction == "None") return "Idle";
    if (self->GetPlayerFlags().isSprinting) return "Sprint";
    return "Run";
}

std::string_view GetMovementTypeLabel(RE::PlayerCharacter* self)
{
    return GetMovementTypeLabel(self, GetMovementDirectionLabel(self));
}

void TickMovementCollection()
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    // Skipped entirely while paused
    if (IsGamePaused()) return;

    ActorContext& ctx = GetActorContext(self);

    std::string_view currentDirection = GetMovementDirectionLabel(self);
    std::string_view currentType = GetMovementTypeLabel(self, currentDirection);

    bool typeChanged = not ctx.lastMovementType.empty() and currentType != ctx.lastMovementType;
    bool directionChanged = not ctx.lastMovementDirection.empty() and currentDirection != ctx.lastMovementDirection;

    float currentHeadingDeg = self->GetAngleZ() * (180.0f / std::numbers::pi_v<float>);
    bool isMoving = currentType != "Idle";
    bool headingChanged = isMoving and not std::isnan(ctx.lastWorldHeadingDeg) and
        std::fabs(AngleDeltaDeg(currentHeadingDeg, ctx.lastWorldHeadingDeg)) >= kWorldHeadingChangeThresholdDeg;

    ctx.lastMovementType = currentType;
    ctx.lastMovementDirection = currentDirection;

    if (isMoving) ctx.lastWorldHeadingDeg = currentHeadingDeg;

    if (typeChanged or directionChanged or headingChanged) CollectData(self, currentType);
}

bool IsCombatActionType(std::string_view actionType)
{
    static constexpr std::string_view kCombatActionTypes[] = 
    {
        "LeftNormalAttack", "RightNormalAttack", "BothNormalAttack",
        "LeftPowerAttack", "RightPowerAttack", "BothPowerAttack",
        "Bash", "CastRight", "CastLeft", "CastBoth", "Block", "BlockRelease",
        "LoadProjectile", "FireProjectile", "CancelProjectile",
    };

    for (std::string_view combatType : kCombatActionTypes)
    {
        if (actionType == combatType) return true;
    }
    return false;
}

bool IsInventoryActionType(std::string_view actionType)
{
    static constexpr std::string_view kInventoryActionTypes[] =
    {
        "SwitchToMeleeLeft", "SwitchToMeleeRight", "SwitchToMeleeBoth",
        "SwitchToRangedLeft", "SwitchToRangedRight", "SwitchToRangedBoth",
        "SwitchToShield",
        "SwitchToSpellLeft", "SwitchToSpellRight",
        "ConsumeRestorative",
    };

    for (std::string_view inventoryType : kInventoryActionTypes)
    {
        if (actionType == inventoryType) return true;
    }
    return false;
}

// Which kind of event produced a snapshot
static std::string_view GetSnapshotTrigger(std::string_view actionType)
{
    if (actionType == kHeartbeatActionType) return "Heartbeat";
    if (IsCombatActionType(actionType)) return "Combat";
    if (IsInventoryActionType(actionType)) return "Inventory";
    if (actionType == "TargetSwitch") return "TargetSwitch";

    // Movement changes, and the first target pick (which passes the movement label)
    return "Movement";
}

// Why the last CollectData call wrote nothing. Empty if it produced rows
static std::string_view g_collectSkipReason;

// A heartbeat that wrote nothing is counted by reason for the combat's summary line, instead of being logged one by one
static void NoteHeartbeatOutcome(std::string_view skipReason)
{
    if (not skipReason.empty()) g_combatLog.skippedHeartbeats[std::string(skipReason)]++;
}

void TickHeartbeatCollection()
{
    auto* self = RE::PlayerCharacter::GetSingleton();
    if (not self) return;

    // Only while the game is running
    if (IsGamePaused()) return;

    if (g_secondsSinceLastSnapshot < kHeartbeatInterval) return;

    // A heartbeat written now would be labelled "None" for a window the player acted in and would get a lower SnapshotID than the action it follows
    if (HasPendingDeferredRows()) return;

    g_secondsSinceLastSnapshot = 0.0f;

    // Only while the player is fighting. Counted only while a combat segment is still open, since that's when it leaves a gap
    if (not self->IsInCombat())
    {
        if (g_inCombat) NoteHeartbeatOutcome("PlayerNotInCombat");
        return;
    }

    CollectData(self, kHeartbeatActionType);
    NoteHeartbeatOutcome(g_collectSkipReason);
}

void CollectData(RE::PlayerCharacter* self, 
    std::string_view actionType,
    bool consumedHealthRestorative, 
    bool consumedStaminaRestorative, 
    bool consumedMagickaRestorative,
    const SelfResourceSample* preConsumptionState,
    std::vector<CombatSnapshotRow>* deferredRows,
    const SelfHandContents* preEquipHands,
    const SelfCombatStateSample* prePressCombatState)
{
    if (deferredRows) deferredRows->clear();

    // Every return below that writes nothing records why, for logging
    g_collectSkipReason = {};

    // Dormant until the Helgen intro is over
    if (not IsModActive())
    {
        g_collectSkipReason = "ModDormant";
        return;
    }

    // Only intentory actions are important when the game is paused
    if (IsGamePaused() and not IsInventoryActionType(actionType))
    {
        g_collectSkipReason = "GamePaused";
        return;
    }

    // Return if player is not in combat
    if (not self->IsInCombat())
    {
        if (g_inCombat) LogCombatEnd("player left combat");

        g_inCombat = false;
        g_markersSeenThisCombat = false;
        g_secondsWithoutMarkers = 0.0f;
        ResetCombatState();
        g_collectSkipReason = "PlayerNotInCombat";
        return;
    }

    if (not g_inCombat)
    {
        g_inCombat = true;
        g_startNewCombat = true;
        g_trackedEnemies.clear();
        g_fallbackTargetFormID = 0;
    }

    // Skip mounted combat
    if (self->IsOnMount())
    {
        g_collectSkipReason = "PlayerMounted";
        return;
    }

    // Skip if "self" is transformed
    if (IsTransformed(self))
    {
        g_collectSkipReason = "PlayerTransformed";
        return;
    }

    // Skip if player has god mode on
    if (RE::PlayerCharacter::IsGodMode())
    {
        g_collectSkipReason = "GodMode";
        return;
    }

    // Skip if shouts are enabled
    if (AreShoutsEnabled())
    {
        g_collectSkipReason = "ShoutsEnabled";
        return;
    }

    // Get Process Lists
    RE::ProcessLists* processLists = RE::ProcessLists::GetSingleton();

    if (not processLists)
    {
        g_collectSkipReason = "NoProcessLists";
        return;
    }

    // Get player context
    ActorContext& selfContext = GetActorContext(self);

    std::uint32_t losCount = 0;
    std::int16_t detectionLevel = processLists->RequestHighestDetectionLevelAgainstActor(self, losCount);

    // Skip if player is not fully detec ted
    if (detectionLevel < 0)
    {
        g_wasFullyDetected = false;
        g_collectSkipReason = "PlayerNotDetected";
        return;
    }
    if (not g_wasFullyDetected)
    {
        g_wasFullyDetected = true;
    }
   
    // List to hold each row for the current snapshot
    std::vector<CombatSnapshotRow> snapshots;

    // Session id
    uint64_t sessionID = g_sessionID;

    // Calculate resoure levels
    std::string_view healthLevel = preConsumptionState ? preConsumptionState->healthLevel
        : CalculateResourceLevel(self, RE::ActorValue::kHealth);
    std::string_view staminaLevel = preConsumptionState ? preConsumptionState->staminaLevel
        : CalculateResourceLevel(self, RE::ActorValue::kStamina);
    std::string_view magickaLevel = preConsumptionState ? preConsumptionState->magickaLevel
        : CalculateResourceLevel(self, RE::ActorValue::kMagicka);

    // Get equipped items for left and right hands
    std::string_view leftType = preEquipHands ? preEquipHands->left
        : GetHandContentName(self->GetEquippedObject(true));
    std::string_view rightType = preEquipHands ? preEquipHands->right
        : GetHandContentName(self->GetEquippedObject(false));

    // Get player's last hit actor
    RE::FormID lastHitActor = selfContext.lastHitActor;

    // Get player's attack phase and blocking state, from before the press for a press-captured action
    const SelfCombatStateSample selfCombatState = prePressCombatState ? *prePressCombatState : SampleCombatState(self);
    AttackPhase selfAttackPhase = selfCombatState.attackPhase;
    bool selfIsBlocking = selfCombatState.isBlocking;

    // A swing that's winding up or releasing can't coexist with being staggered
    const bool selfIsStartingAttack = selfAttackPhase == AttackPhase::kWindup or selfAttackPhase == AttackPhase::kRelease;

    // Check if player is staggered
    bool selfIsStaggered = self->IsStaggering() and not selfIsStartingAttack;

    // Check if player is currently casting
    bool selfIsCasting = IsActorCasting(self);

    // Check if the player currently has a flesh-type Alteration armor spell active
    bool selfHasFleshSpellActive = HasFleshSpellActive(self);

    std::string_view movementDirection = GetMovementDirectionLabel(self);
    std::string_view movementType = GetMovementTypeLabel(self, movementDirection);

    bool selfIsJumping = movementType == "Jump";
    bool selfIsSprinting = movementType == "Sprint";
    bool selfIsRunning = movementType == "Run";
    bool selfIsIdle = movementType == "Idle";

    // Check if a hostile projectile is currently in flight toward the player
    bool selfHasIncomingProjectileThreat = IsProjectileIncomingAtSelf(self);
    std::string_view selfIncomingProjectileDistance = GetIncomingProjectileDistanceRepresentation(self);

    // Check which magic schools the player knows at least one spell in
    KnownSpellSchools selfSpellSchools = GetKnownSpellSchools(self);
    bool selfKnowsADamageSpell = selfSpellSchools.destructionContinuous
        or selfSpellSchools.destructionProjectile
        or selfSpellSchools.destructionRune
        or selfSpellSchools.destructionCloak;
    bool selfKnowsAnArmorSpell = selfSpellSchools.armor;
    bool selfKnowsAHealingSpell = selfSpellSchools.restorationHealing;
    bool selfKnowsAWardSpell = selfSpellSchools.restorationWard;
    bool selfKnowsAConjurationFollowerSpell = selfSpellSchools.conjurationFollower;
    bool selfKnowsAConjurationWeaponSpell = selfSpellSchools.conjurationWeapon;

    // Check which restorative potions/food the player has available
    bool selfHasHealthRestorative = preConsumptionState ? preConsumptionState->hasHealthRestorative
        : HasRestorative(self, RestoreType::kHealth);
    bool selfHasStaminaRestorative = preConsumptionState ? preConsumptionState->hasStaminaRestorative
        : HasRestorative(self, RestoreType::kStamina);
    bool selfHasMagickaRestorative = preConsumptionState ? preConsumptionState->hasMagickaRestorative
        : HasRestorative(self, RestoreType::kMagicka);

    // Check which weapon categories the player has available to switch to
    InventoryWeaponFlags inventoryWeaponFlags = GetInventoryWeaponFlags(self);
    bool selfHasOneHandedMeleeInInventory = inventoryWeaponFlags.oneHandedMelee;
    bool selfHasTwoHandedMeleeInInventory = inventoryWeaponFlags.twoHandedMelee;
    bool selfHasOneHandedRangedInInventory = inventoryWeaponFlags.oneHandedRanged;
    bool selfHasTwoHandedRangedInInventory = inventoryWeaponFlags.twoHandedRanged;
    bool selfHasShieldInInventory = inventoryWeaponFlags.shield;
    std::string_view selfArrowCount = inventoryWeaponFlags.arrowCountLevel;
    std::string_view selfBoltCount = inventoryWeaponFlags.boltCountLevel;

    // Check the player's body armor type
    std::string_view selfArmorType = GetBodyArmorType(self);

    // Check if player is aiming a ranged weapond
    bool selfIsAimingRangedWeapon = selfCombatState.isAimingRanged;

    // Player level
    std::string_view selfLevel = CalculateCharacterLevel(self->GetLevel());

    // Skill levels use the base value
    auto* selfAVOwner = self->AsActorValueOwner();
    std::string_view selfHeavyArmorLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kHeavyArmor));
    bool selfHasFistsOfSteelPerk = HasFistsOfSteelPerk(self);

    std::string_view selfLightArmorLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kLightArmor));

    std::string_view selfBlockLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kBlock));
    bool selfHasElementalProtectionPerk = HasElementalProtectionPerk(self);
    bool selfHasBlockRunnerPerk = HasBlockRunnerPerk(self);
    bool selfHasShieldChargePerk = HasShieldChargePerk(self);
    bool selfHasDisarmingBashPerk = HasDisarmingBashPerk(self);

    std::string_view selfTwoHandedLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kTwoHanded));
    bool selfHasGreatCriticalChargePerk = HasGreatCriticalChargePerk(self);

    std::string_view selfOneHandedLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kOneHanded));
    bool selfHasCriticalChargePerk = HasCriticalChargePerk(self);
    bool selfHasDualSavageryPerk = HasDualSavageryPerk(self);

    std::string_view selfArcheryLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kArchery));

    std::string_view selfIllusionLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kIllusion));
    bool selfHasIllusionDualCastingPerk = HasIllusionDualCastingPerk(self);

    std::string_view selfConjurationLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kConjuration));
    bool selfHasConjurationDualCastingPerk = HasConjurationDualCastingPerk(self);
    bool selfHasMysticBindingPerk = HasMysticBindingPerk(self);
    bool selfHasTwinSoulsPerk = HasTwinSoulsPerk(self);

    std::string_view selfDestructionLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kDestruction));
    bool selfHasDestructionDualCastingPerk = HasDestructionDualCastingPerk(self);
    bool selfHasImpactPerk = HasImpactPerk(self);

    std::string_view selfRestorationLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kRestoration));
    bool selfHasRestorationDualCastingPerk = HasRestorationDualCastingPerk(self);

    std::string_view selfAlterationLevel = CalculateSkillLevel(selfAVOwner->GetBaseActorValue(RE::ActorValue::kAlteration));
    bool selfHasAlterationDualCastingPerk = HasAlterationDualCastingPerk(self);

    // Template row carrying every identifier/trigger/player-level field computed above — copied once per enemy
    CombatSnapshotRow rowTemplate{};
    rowTemplate.sessionID = sessionID;
    rowTemplate.snapshotID = 0;
    rowTemplate.gameTimeMs = static_cast<uint64_t>(g_gameTimeSeconds * 1000.0);
    rowTemplate.snapshotTrigger = GetSnapshotTrigger(actionType);
    rowTemplate.combatActionType = IsCombatActionType(actionType) ? actionType : "None";
    rowTemplate.inventoryActionType = IsInventoryActionType(actionType) ? actionType : "None";

    // For a spell switch, which category of spell ended up equipped in the hand that changed.
    rowTemplate.spellCategory = "None";
    if (actionType.starts_with("SwitchToSpell"))
    {
        const bool leftHand = actionType != "SwitchToSpellRight";
        rowTemplate.spellCategory = GetSpellCategoryName(self->GetEquippedObject(leftHand));
    }
    rowTemplate.movementActionType = movementType;
    rowTemplate.consumedHealthRestorative = consumedHealthRestorative;
    rowTemplate.consumedStaminaRestorative = consumedStaminaRestorative;
    rowTemplate.consumedMagickaRestorative = consumedMagickaRestorative;
    rowTemplate.healthLevel = healthLevel;
    rowTemplate.staminaLevel = staminaLevel;
    rowTemplate.magickaLevel = magickaLevel;
    rowTemplate.leftType = leftType;
    rowTemplate.rightType = rightType;
    rowTemplate.selfAttackPhase = AttackPhaseToString(selfAttackPhase);
    rowTemplate.selfIsBlocking = selfIsBlocking;
    rowTemplate.selfIsStaggered = selfIsStaggered;
    rowTemplate.selfIsCasting = selfIsCasting;
    rowTemplate.selfHasFleshSpellActive = selfHasFleshSpellActive;
    rowTemplate.selfIsRunning = selfIsRunning;
    rowTemplate.selfIsSprinting = selfIsSprinting;
    rowTemplate.selfIsIdle = selfIsIdle;
    rowTemplate.selfIsJumping = selfIsJumping;
    rowTemplate.movementDirection = movementDirection;
    rowTemplate.selfHasIncomingProjectileThreat = selfHasIncomingProjectileThreat;
    rowTemplate.selfIncomingProjectileDistance = selfIncomingProjectileDistance;
    rowTemplate.selfKnowsADamageSpell = selfKnowsADamageSpell;
    rowTemplate.selfKnowsAnArmorSpell = selfKnowsAnArmorSpell;
    rowTemplate.selfKnowsAHealingSpell = selfKnowsAHealingSpell;
    rowTemplate.selfKnowsAWardSpell = selfKnowsAWardSpell;
    rowTemplate.selfKnowsAConjurationFollowerSpell = selfKnowsAConjurationFollowerSpell;
    rowTemplate.selfKnowsAConjurationWeaponSpell = selfKnowsAConjurationWeaponSpell;
    rowTemplate.selfHasHealthRestorative = selfHasHealthRestorative;
    rowTemplate.selfHasStaminaRestorative = selfHasStaminaRestorative;
    rowTemplate.selfHasMagickaRestorative = selfHasMagickaRestorative;
    rowTemplate.selfHasOneHandedMeleeInInventory = selfHasOneHandedMeleeInInventory;
    rowTemplate.selfHasTwoHandedMeleeInInventory = selfHasTwoHandedMeleeInInventory;
    rowTemplate.selfHasOneHandedRangedInInventory = selfHasOneHandedRangedInInventory;
    rowTemplate.selfHasTwoHandedRangedInInventory = selfHasTwoHandedRangedInInventory;
    rowTemplate.selfHasShieldInInventory = selfHasShieldInInventory;
    rowTemplate.selfArrowCount = selfArrowCount;
    rowTemplate.selfBoltCount = selfBoltCount;
    rowTemplate.selfArmorType = selfArmorType;
    rowTemplate.selfIsAimingRangedWeapon = selfIsAimingRangedWeapon;

    rowTemplate.selfLevel = selfLevel;
    rowTemplate.selfHeavyArmorLevel = selfHeavyArmorLevel;
    rowTemplate.selfHasFistsOfSteelPerk = selfHasFistsOfSteelPerk;
    rowTemplate.selfLightArmorLevel = selfLightArmorLevel;
    rowTemplate.selfBlockLevel = selfBlockLevel;
    rowTemplate.selfHasElementalProtectionPerk = selfHasElementalProtectionPerk;
    rowTemplate.selfHasBlockRunnerPerk = selfHasBlockRunnerPerk;
    rowTemplate.selfHasShieldChargePerk = selfHasShieldChargePerk;
    rowTemplate.selfHasDisarmingBashPerk = selfHasDisarmingBashPerk;
    rowTemplate.selfTwoHandedLevel = selfTwoHandedLevel;
    rowTemplate.selfHasGreatCriticalChargePerk = selfHasGreatCriticalChargePerk;
    rowTemplate.selfOneHandedLevel = selfOneHandedLevel;
    rowTemplate.selfHasCriticalChargePerk = selfHasCriticalChargePerk;
    rowTemplate.selfHasDualSavageryPerk = selfHasDualSavageryPerk;
    rowTemplate.selfArcheryLevel = selfArcheryLevel;
    rowTemplate.selfIllusionLevel = selfIllusionLevel;
    rowTemplate.selfHasIllusionDualCastingPerk = selfHasIllusionDualCastingPerk;
    rowTemplate.selfConjurationLevel = selfConjurationLevel;
    rowTemplate.selfHasConjurationDualCastingPerk = selfHasConjurationDualCastingPerk;
    rowTemplate.selfHasMysticBindingPerk = selfHasMysticBindingPerk;
    rowTemplate.selfHasTwinSoulsPerk = selfHasTwinSoulsPerk;
    rowTemplate.selfDestructionLevel = selfDestructionLevel;
    rowTemplate.selfHasDestructionDualCastingPerk = selfHasDestructionDualCastingPerk;
    rowTemplate.selfHasImpactPerk = selfHasImpactPerk;
    rowTemplate.selfRestorationLevel = selfRestorationLevel;
    rowTemplate.selfHasRestorationDualCastingPerk = selfHasRestorationDualCastingPerk;
    rowTemplate.selfAlterationLevel = selfAlterationLevel;
    rowTemplate.selfHasAlterationDualCastingPerk = selfHasAlterationDualCastingPerk;

    // Counters for enemies and allies
    int enemyCount = 0;  // Total enemies in combat
    int allyCount = 0;  // Total allies in combat

    // Whether at least one enemy in this fight is humanoid. 
    // A fight with only creatures isn't representative data, since the model will always control a humanoid NPC and the player is always its enemy
    bool anyHumanoidEnemy = false;

    // A fight involving an essential enemy isn't representative training data
    bool anyEssentialEnemy = false;

    // Compute player's range
    float selfMaxRange = GetMaximumActorRange(self);

    // Each enemy row's actual distance, in the same order as the rows. Only used to choose which enemies to keep
    std::vector<float> enemyDistances;

    // Scoped to highActorHandles only
    processLists->ForEachHighActor([&](RE::Actor* enemyActor) -> RE::BSContainer::ForEachResult
    {
        // Skip if NPC actor is null or is the player
        if (not enemyActor or not enemyActor->Get3D() or enemyActor == self)
        {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        // Get NPC FormID
        RE::FormID enemyFormID = enemyActor->GetFormID();

        // Check if NPC actor is an ally or enemy
        bool isEnemy = enemyActor->IsHostileToActor(self) or self->IsHostileToActor(enemyActor);

        // An essential enemy invalidates the whole combat
        if (isEnemy and enemyActor->IsEssential())
        {
            anyEssentialEnemy = true;
            return RE::BSContainer::ForEachResult::kContinue;
        }

        const bool enemyIsBleedingOut = enemyActor->AsActorState()->IsBleedingOut();

        // Skip dead actors
        if (not IsAliveOrBleedingOut(enemyActor))
        {
            return RE::BSContainer::ForEachResult::kContinue;
        }

        // Camera-related heading
        const RE::NiBound& enemyBound = enemyActor->Get3D()->worldBound;
        float enemyCameraHeading = GetCameraRelativeAngle(enemyBound.center);

        
        // Check if player has line of sight to enemy
        bool ignoredLOS = false;
        bool enemyHasLineOfSight = self->HasLineOfSight(enemyActor, ignoredLOS);

        // Check if enemy is shown as red dot marker
        bool enemyShownAsEnemyMarker = EnemyMarkerHook::IsShownAsEnemyMarker(enemyFormID);

        // Check if enemy red marker is currently visible in the compass
        bool enemyInCompassRange = enemyShownAsEnemyMarker
            and std::abs(enemyCameraHeading) <= kCompassHalfWidthDegrees;

        // Check if enemy is in-camera
        bool enemyInCameraFrustum = false;
        if (auto* worldCamera = RE::Main::WorldRootCamera())
        {
            enemyInCameraFrustum = worldCamera->PointInFrustum(enemyBound.center, enemyBound.radius);
        }

        // The player sees an enemy if htey have line of sight and they are either in-camera or shown in the compass
        bool enemyIsVisible = enemyHasLineOfSight and (enemyInCompassRange or enemyInCameraFrustum);

        // Enemy context
        ActorContext& enemyCtx = GetActorContext(enemyActor);

        // Remember that the player has seen this actor at some point
        if (enemyIsVisible) enemyCtx.everSeen = true;

        if (isEnemy)
        {
            bool enemyIsRealMarkerCandidate = enemyShownAsEnemyMarker and enemyIsVisible;

            if (enemyIsRealMarkerCandidate) enemyCtx.inCombat = true;

            // Once true, stays true for the rest of this combat
            if (not enemyCtx.inCombat) return RE::BSContainer::ForEachResult::kContinue;
        }
        else
        {
            // Allies still require IsInCombat()
            if (not enemyActor->IsInCombat()) return RE::BSContainer::ForEachResult::kContinue;
        }

        // Count enemies and allies
        if (isEnemy) enemyCount++;
        else allyCount++;

        // Do not collect data for allies
        if (not isEnemy) return RE::BSContainer::ForEachResult::kContinue;

        // Get NPC's race
        auto* race = enemyActor->GetRace();

        if (!race) return RE::BSContainer::ForEachResult::kContinue;

        // Get NPC race ID
        auto raceEditorID = race->GetFormEditorID();

        if (not raceEditorID) return RE::BSContainer::ForEachResult::kContinue;

        // Check if NPC's race is humanoid
        bool enemyIsHumanoid = IsHumanoidRace(raceEditorID);

        if (enemyIsHumanoid) anyHumanoidEnemy = true;

        // Only used for debugging
        std::string_view enemyName = enemyActor->GetName();

        // Check if current NPC actor was the actor that the player attacked most recently
        bool wasLastHitEnemy = lastHitActor == enemyFormID;

        // Angle can be interpreted from the compass - does not require actual visibility
        std::string_view angle = enemyInCompassRange or (enemyInCameraFrustum and enemyHasLineOfSight)
            ? AngleToString(enemyCameraHeading)
            : "Unknown";

        // Check if current enemy NPC is blocking
        bool isBlocking = enemyIsVisible and enemyActor->IsBlocking();

        // Get the enemy's current combat target. Used to collect other features that are targeting the player
        auto targetHandle = enemyActor->GetActorRuntimeData().currentCombatTarget;
        RE::Actor* combatTarget = targetHandle ? targetHandle.get().get() : nullptr;
        bool targetingSelf = enemyIsVisible && combatTarget == self;

        // Check if enemy is attacking the player
        AttackPhase attackPhase = enemyIsVisible and targetingSelf ? GetAttackPhase(enemyActor) : AttackPhase::kNone;

        // Check if enemy is power attacking the player
        bool isPowerAttackingSelf = enemyIsVisible and targetingSelf and attackPhase != AttackPhase::kNone
            and IsPowerAttacking(enemyActor);

        // Check if enemy is doing a normal attack
        bool isNormalAttackingSelf = attackPhase != AttackPhase::kNone and not isPowerAttackingSelf;

        // Check if enemy archer is visibly aiming a bow/crossbow at the player
        bool enemyIsAimingAtSelf = enemyIsVisible and targetingSelf and IsAimingRangedWeapon(enemyActor);

        // Whether this is the NPC currently shown in Skyrim's enemy health-bar HUD
        bool isSelfTarget = enemyFormID == selfContext.lastCrosshairTarget;

        // Calculate NPC health only for the NPC whose health is shown in the HUD
        std::string_view enemyHealthLevel = isSelfTarget ? CalculateResourceLevel(enemyActor, RE::ActorValue::kHealth) : "Unknown";

        // Get actor's weapons. These are remembered even if visibility drops
        if (enemyIsVisible)
        {
            enemyCtx.observedLeftType = enemyIsHumanoid
                ? GetHandContentName(enemyActor->GetEquippedObject(true)) : "Empty";
            enemyCtx.observedRightType = enemyIsHumanoid
                ? GetHandContentName(enemyActor->GetEquippedObject(false)) : "Empty";
        }
        std::string_view actorLeftType = enemyCtx.observedLeftType;
        std::string_view actorRightType = enemyCtx.observedRightType;

        // Summon/reanimation status and its commander
        if (enemyIsVisible)
        {
            auto commander = enemyActor->GetCommandingActor();
            enemyCtx.observedSummonerFormID = commander ? commander->GetFormID() : 0;
            enemyCtx.observedIsSummoned = enemyActor->IsSummoned() or commander != nullptr;
        }

        // Check the NPC's body armor type. This is remembered even if visibility drops
        if (enemyIsVisible) enemyCtx.armorType = GetBodyArmorType(enemyActor);
        std::string_view enemyArmorType = enemyCtx.armorType;

        // Check if NPC is staggered
        bool enemyIsStaggered = enemyIsVisible and enemyActor->IsStaggering();

        // Check if NPC is casting a spell
        bool enemyIsCasting = enemyIsVisible and targetingSelf and IsActorCasting(enemyActor);

        // Run/sprint only describe grounded locomotion
        bool enemyIsGrounded = enemyIsVisible and not enemyActor->IsFlying();

        // Collapsed to Jump/Sprint/Run/Idle
        float enemySpeed = 0.0f;
        float enemySpeedRun = 0.0f;
        if (enemyIsGrounded)
        {
            enemyActor->GetGraphVariableFloat("Speed", enemySpeed);
            enemyActor->GetGraphVariableFloat("SpeedRun", enemySpeedRun);
        }

        // Check if enemy is jumping
        bool enemyIsJumping = enemyIsGrounded and enemyActor->IsInJumpState();

        // Check if enemy is sprinting
        bool enemyIsSprinting = enemyIsGrounded and not enemyIsBleedingOut and not enemyIsJumping and enemySpeedRun > 0.0f
            and enemySpeed > enemySpeedRun * kSprintSpeedRatio;

        // Check if enemy is running
        bool enemyIsRunning = enemyIsGrounded and not enemyIsBleedingOut and not enemyIsJumping and not enemyIsSprinting and enemySpeed > 0.0f;

        // Check if enemy is idle
        bool enemyIsIdle = enemyIsGrounded and not enemyIsJumping and not enemyIsSprinting and not enemyIsRunning;

        // Get distance to NPC
        std::string_view distanceToEnemy = enemyInCameraFrustum and enemyHasLineOfSight
            ? GetDistanceRepresentation(GetDistance(self, enemyActor), selfMaxRange) : "Unknown";

        // Whether the NPC stands above, below or level with the player, feet to feet. Observable under the same conditions as the distance
        std::string_view enemyHeight = "Unknown";
        if (enemyInCameraFrustum and enemyHasLineOfSight)
        {
            const float heightDifference = enemyActor->GetPosition().z - self->GetPosition().z;
            if (heightDifference > kSameLevelHeightTolerance) enemyHeight = "above";
            else if (heightDifference < -kSameLevelHeightTolerance) enemyHeight = "below";
            else enemyHeight = "same_level";
        }

        // Whether the NPC's own body is oriented toward the player
        bool enemyIsFacingSelf = enemyInCameraFrustum and enemyHasLineOfSight
            and GetAngleRepresentation(enemyActor, self) == "front";

        // Static name-based strength — sticky, refreshed only while the NPC is in the HUD, since
        // that's when the player actually sees the exact name ("Bandit" vs. "Bandit Marauder").
        if (isSelfTarget) enemyCtx.observedStrengthLevel = GetEnemyStrengthLevel(enemyActor, selfLevel);
        std::string_view enemyStrengthLevel = enemyCtx.observedStrengthLevel;

        // Start writing row
        CombatSnapshotRow row = rowTemplate;
        row.enemyFormID = enemyFormID;
        row.enemyName = enemyName;
        row.enemyStrengthLevel = enemyStrengthLevel;
        row.enemyIsSummoned = enemyCtx.observedIsSummoned;
        row.enemySummonerFormID = enemyCtx.observedSummonerFormID;
        row.wasLastHitEnemy = wasLastHitEnemy;
        row.enemyAngle = angle;
        row.enemyIsVisible = enemyIsVisible;

        // Every state flag below is only observable while the NPC is visible
        // When it isn't, it's "Unknown" rather than a false-looking 0.
        row.enemyIsBlocking = BoolOrUnknown(enemyIsVisible, isBlocking);
        row.enemyAttackPhase = enemyIsVisible ? AttackPhaseToString(attackPhase) : "Unknown";
        row.enemyIsBleedingOut = BoolOrUnknown(enemyIsVisible, enemyIsBleedingOut);
        row.enemyIsPowerAttackingSelf = BoolOrUnknown(enemyIsVisible, isPowerAttackingSelf);
        row.enemyIsNormalAttackingSelf = BoolOrUnknown(enemyIsVisible, isNormalAttackingSelf);
        row.enemyIsAimingAtSelf = BoolOrUnknown(enemyIsVisible, enemyIsAimingAtSelf);
        row.enemyIsSelfTarget = isSelfTarget;
        row.enemyHealthLevel = enemyHealthLevel;
        row.enemyLeftType = actorLeftType;
        row.enemyRightType = actorRightType;
        row.enemyArmorType = enemyArmorType;
        row.enemyIsStaggered = BoolOrUnknown(enemyIsVisible, enemyIsStaggered);
        row.enemyIsCasting = BoolOrUnknown(enemyIsVisible, enemyIsCasting);
        row.enemyIsGrounded = BoolOrUnknown(enemyIsVisible, enemyIsGrounded);
        row.enemyIsRunning = BoolOrUnknown(enemyIsVisible, enemyIsRunning);
        row.enemyIsSprinting = BoolOrUnknown(enemyIsVisible, enemyIsSprinting);
        row.enemyIsIdle = BoolOrUnknown(enemyIsVisible, enemyIsIdle);
        row.enemyIsJumping = BoolOrUnknown(enemyIsVisible, enemyIsJumping);
        row.enemyDistance = distanceToEnemy;
        row.enemyHeight = enemyHeight;
        row.enemyIsFacingSelf = BoolOrUnknown(enemyInCameraFrustum and enemyHasLineOfSight, enemyIsFacingSelf);

        snapshots.push_back(row);
        enemyDistances.push_back(GetDistance(self, enemyActor));

        return RE::BSContainer::ForEachResult::kContinue;
    });

    // If at least one enemy was essential, write nothing
    if (anyEssentialEnemy)
    {
        g_collectSkipReason = "EssentialEnemy";
        return;
    }

    // Nothing worth collecting if every enemy present is a creature. No enemy at all means none is both marked and seen yet
    if (!anyHumanoidEnemy)
    {
        g_collectSkipReason = snapshots.empty() ? "NoTrackedEnemy" : "OnlyCreatureEnemies";
        return;
    }

    // The NPC always has a combat target, so the player's movement frame needs an enemy too: the HUD target or, without one, the nearest visible enemy
    std::string_view targetSource;
    const RE::FormID frameTargetFormID = ChooseFrameTarget(snapshots, enemyDistances, targetSource);
    if (frameTargetFormID == 0)
    {
        g_collectSkipReason = "NoTargetOrVisibleEnemy";
        return;
    }

    // Restarted at capture time, so a deferred action counts from when it happened, not when it's written
    if (not snapshots.empty()) g_secondsSinceLastSnapshot = 0.0f;

    // Big battles: rows for at most kMaxTrackedEnemies enemies. enemyCount below still counts every enemy in the fight
    KeepTrackedEnemies(snapshots, enemyDistances, frameTargetFormID);

    // Free space and drops around the player and the movement direction, all relative to the frame target ("forward" points at it).
    // Walls and drops only count what the player can see: a direction whose wall (or end of free space, or floor) is outside the camera's view is "Unknown"
    RE::Actor* frameTarget = RE::TESForm::LookupByID<RE::Actor>(frameTargetFormID);
    const Surroundings selfSurroundings = GetSurroundings(self, frameTarget, IsInPlayerCameraView);
    const std::string_view movementDirectionToTarget = GetMovementDirectionToTargetLabel(self, frameTarget);

    // Write global features to all rows
    for (auto& row : snapshots)
    {
        row.targetSource = targetSource;
        row.enemyIsFrameTarget = row.enemyFormID == frameTargetFormID;
        row.movementDirectionToTarget = movementDirectionToTarget;
        row.selfClearance = selfSurroundings.clearance;
        row.selfDrop = selfSurroundings.drop;
        row.enemyCount = enemyCount;
        row.allyCount = allyCount;

        // A summoner's identity is only known to the player once they've actually seen the summoner
        row.enemySummonerKnown = row.enemySummonerFormID == 0 or HasActorBeenSeen(row.enemySummonerFormID);
    }

    // WRite held-out rows
    if (deferredRows)
    {
        *deferredRows = std::move(snapshots);
        return;
    }

    WriteSnapshotRows(snapshots);
}

void CommitDeferredRows(std::vector<CombatSnapshotRow>& rows, std::string_view combatActionType)
{
    for (auto& row : rows)
    {
        row.combatActionType = combatActionType;
    }
    WriteSnapshotRows(rows);
    rows.clear();
}