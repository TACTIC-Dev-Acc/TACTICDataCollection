#include "Managers/GeometryManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
    // About 0.7 m above the feet: catches walls, furniture and rocks, passes over small clutter
    constexpr float kRayHeight = 48.0f;

    // About 70 units per metre. Clearance is "blocked" under 1 m, "near" under 3 m, "open" beyond
    constexpr float kBlockedDistance = 70.0f;
    constexpr float kOpenDistance = 210.0f;

    // How far out the ground is probed, and how far from the feet it can be and still count as level.
    // About 4 m: in first person, with the eyes ~1.7 m up and ~25 degrees of view below the horizon, the floor only comes into view ~3.7 m ahead
    constexpr float kDropProbeDistance = 280.0f;
    constexpr float kLevelTolerance = 35.0f;

    // The horizontal ray reaches a little past the drop probe, so a wall right at the probe point still counts as in front of it
    constexpr float kRayLength = kDropProbeDistance + 35.0f;

    // How much deeper than the fall-damage height the probe looks, so a drop at that height is still hit
    constexpr float kDropProbeMargin = 70.0f;

    // Height of a fall that starts dealing damage, from the game's own setting
    float GetFallDamageHeight()
    {
        static float height = []
        {
            auto* settings = RE::GameSettingCollection::GetSingleton();
            auto* setting = settings ? settings->GetSetting("fJumpFallHeightMin") : nullptr;
            return setting ? setting->GetFloat() : 0.0f;
        }();
        return height;
    }

    // Actors' bodies. The line-of-sight layer hits them, but enemies aren't walls or ledges, and the target is usually right in front of the player
    bool IsCharacterLayer(RE::COL_LAYER layer)
    {
        return layer == RE::COL_LAYER::kBiped or layer == RE::COL_LAYER::kCharController
            or layer == RE::COL_LAYER::kBipedNoCC or layer == RE::COL_LAYER::kDeadBip;
    }

    // How far along a ray the first wall/terrain/static hit is, from 0 (at the start) to 1 (nothing hit). Characters are passed through
    float CastRay(RE::bhkWorld* world, const RE::NiPoint3& from, const RE::NiPoint3& to, const RE::CFilter& filter)
    {
        const float scale = RE::bhkWorld::GetWorldScale();
        const RE::NiPoint3 fullRay = to - from;

        // Each character hit restarts the ray just past it. A crowd can stack several, so a few retries
        constexpr int kMaxCharacterSkips = 8;
        constexpr float kSkipStep = 2.0f;

        float travelled = 0.0f;
        for (int attempt = 0; attempt <= kMaxCharacterSkips; attempt++)
        {
            const RE::NiPoint3 start = from + fullRay * travelled;

            RE::bhkPickData pick;
            pick.rayInput.from = RE::hkVector4(start.x * scale, start.y * scale, start.z * scale, 0.0f);
            pick.rayInput.to = RE::hkVector4(to.x * scale, to.y * scale, to.z * scale, 0.0f);
            pick.rayInput.filterInfo = filter;

            world->PickObject(pick);
            if (not pick.rayOutput.HasHit()) return 1.0f;

            // The hit as a fraction of the whole ray, not just of the remaining part
            const float hit = travelled + pick.rayOutput.hitFraction * (1.0f - travelled);
            if (not IsCharacterLayer(pick.rayOutput.rootCollidable->GetCollisionLayer())) return hit;

            // Past the character, by a couple of units so it isn't hit again
            const float length = fullRay.Length();
            if (length <= 0.0f) return 1.0f;
            travelled = hit + kSkipStep / length;
            if (travelled >= 1.0f) return 1.0f;
        }

        // Too many characters in the way to see past them
        return 1.0f;
    }
}

bool IsInPlayerCameraView(const RE::NiPoint3& a_point)
{
    auto* camera = RE::Main::WorldRootCamera();
    return camera and camera->PointInFrustum(a_point, 0.0f);
}

Surroundings GetSurroundings(RE::Actor* actor, RE::Actor* target, IsPointObservableFn isObservable)
{
    Surroundings surroundings;
    surroundings.clearance.fill("Unknown");
    surroundings.drop.fill("Unknown");

    auto* cell = actor ? actor->GetParentCell() : nullptr;
    auto* world = cell ? cell->GetbhkWorld() : nullptr;
    if (not world) return surroundings;

    auto canSee = [isObservable](const RE::NiPoint3& point) { return not isObservable or isObservable(point); };

    // The actor's own system group, so the rays ignore its own capsule, on the line-of-sight layer, which hits statics and terrain but not characters
    RE::CFilter actorFilter;
    actor->GetCollisionFilterInfo(actorFilter);
    RE::CFilter rayFilter;
    rayFilter.filter = (actorFilter.filter & 0xFFFF0000) | static_cast<std::uint32_t>(RE::COL_LAYER::kLOS);

    const RE::NiPoint3 feet = actor->GetPosition();
    const RE::NiPoint3 origin{ feet.x, feet.y, feet.z + kRayHeight };

    // A drop at least this deep is dangerous. Without the setting, every drop past level counts as one
    const float fallDamageHeight = GetFallDamageHeight();
    const float dropProbeDepth = fallDamageHeight + kDropProbeMargin;

    // Skyrim headings are clockwise from +Y (north), so a heading h points along (sin h, cos h)
    float forwardHeading = actor->GetAngleZ();
    if (target and target != actor)
    {
        const RE::NiPoint3 toTarget = target->GetPosition() - feet;
        forwardHeading = std::atan2(toTarget.x, toTarget.y);
    }

    for (std::size_t i = 0; i < kRelativeDirections.size(); i++)
    {
        const float heading = forwardHeading + static_cast<float>(i) * (std::numbers::pi_v<float> / 4.0f);
        const RE::NiPoint3 direction{ std::sin(heading), std::cos(heading), 0.0f };

        // --- Clearance: where the horizontal ray ended (what was hit, or the far end of free space) is only known if it can be seen ---
        const float wallDistance = CastRay(world, origin, origin + direction * kRayLength, rayFilter) * kRayLength;

        // Free space only needs to be seen out to the "open" distance
        if (canSee(origin + direction * std::min(wallDistance, kOpenDistance)))
        {
            if (wallDistance < kBlockedDistance) surroundings.clearance[i] = "blocked";
            else if (wallDistance < kOpenDistance) surroundings.clearance[i] = "near";
            else surroundings.clearance[i] = "open";
        }

        // --- Drop: a wall before the probe point means the probe would land beyond it ---
        if (wallDistance < kRayLength)
        {
            if (canSee(origin + direction * wallDistance)) surroundings.drop[i] = "blocked";
            continue;
        }

        // Whether there's ground is only known if the spot where the floor should be can be seen. A visible edge is visible whatever lies beyond it
        const RE::NiPoint3 probe = feet + direction * kDropProbeDistance;
        if (not canSee(probe)) continue;

        const RE::NiPoint3 probeTop{ probe.x, probe.y, origin.z };
        const RE::NiPoint3 probeBottom{ probe.x, probe.y, feet.z - dropProbeDepth };
        const float groundFraction = CastRay(world, probeTop, probeBottom, rayFilter);

        // No ground at all: the edge of a void
        if (groundFraction >= 1.0f)
        {
            surroundings.drop[i] = "drop";
            continue;
        }

        const RE::NiPoint3 ground = probeTop + (probeBottom - probeTop) * groundFraction;
        const float depth = feet.z - ground.z;

        if (depth < kLevelTolerance) surroundings.drop[i] = "level";

        // An edge whose bottom can't be seen is treated as dangerous, as a player would
        else if (not canSee(ground) or depth >= fallDamageHeight) surroundings.drop[i] = "drop";
        else surroundings.drop[i] = "step";
    }

    return surroundings;
}
