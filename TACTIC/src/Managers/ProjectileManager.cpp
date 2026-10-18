#include "Managers/ProjectileManager.h"
#include "Managers/DistanceManager.h"

#include <optional>

namespace {
    // Comparing dot products of unit vectors instead of computing an actual angle.
    // 0.9397f == cos(20 degrees): wide enough to aslo flag AoE spells
    constexpr float kHeadingToleranceCos = 0.9397f;

    // Ignore very distant projectiles
    constexpr float kMaxThreatDistance = 3000.0f;

    bool IsHeadingTowardSelf(RE::Projectile* projectile, RE::PlayerCharacter* self) 
    {
        RE::NiPoint3 toSelf = self->GetPosition() - projectile->GetPosition();
        const float distance = toSelf.Length();

        if (distance <= 0.001f || distance > kMaxThreatDistance) return false;

        RE::NiPoint3 velocity{};
        projectile->GetLinearVelocity(velocity);

        if (velocity.Length() <= 0.001f) return false;

        toSelf.Unitize();
        velocity.Unitize();

        // Angle between the projectile's travel direction and the direction to the player
        return toSelf.Dot(velocity) >= kHeadingToleranceCos;
    }

    // Only keeps the closest projectile
    std::optional<float> ClosestIncomingIn(const RE::BSTArray<RE::ProjectileHandle>& handles,
        RE::PlayerCharacter* self, std::optional<float> best) 
    {
        for (auto& handle : handles) {
            RE::Projectile* projectile = handle.get().get();
            if (!projectile) continue;

            auto& runtimeData = projectile->GetProjectileRuntimeData();
            auto* shooterRef = runtimeData.shooter.get().get();
            auto* shooter = shooterRef ? shooterRef->As<RE::Actor>() : nullptr;

            // Ignore player projectiles
            if (!shooter || shooter == self) continue;
            if (!shooter->IsHostileToActor(self)) continue;

            auto* spell = runtimeData.spell;

            auto* base = projectile->GetProjectileBase();
            if (!base or not(base->IsMissile() or base->IsGrenade() or base->IsCone())) continue;

            // Ignore continuous spells
            if (spell and spell->GetCastingType() == RE::MagicSystem::CastingType::kConcentration) continue;

            if (!IsHeadingTowardSelf(projectile, self)) continue;

            float distance = self->GetPosition().GetDistance(projectile->GetPosition());
            if (!best or distance < *best) best = distance;
        }
        return best;
    }

    std::optional<float> GetClosestIncomingProjectileDistance(RE::PlayerCharacter* self) 
    {
        if (!self) return std::nullopt;

        auto* manager = RE::Projectile::Manager::GetSingleton();
        if (!manager) return std::nullopt;

        std::optional<float> best = ClosestIncomingIn(manager->unlimited, self, std::nullopt);
        return ClosestIncomingIn(manager->limited, self, best);
    }
}

bool IsProjectileIncomingAtSelf(RE::PlayerCharacter* self)
{
    return GetClosestIncomingProjectileDistance(self).has_value();
}

std::string_view GetIncomingProjectileDistanceRepresentation(RE::PlayerCharacter* self) 
{
    std::optional<float> distance = GetClosestIncomingProjectileDistance(self);
    if (!distance) return "Unknown";

    return GetDistanceRepresentation(self, *distance);
}
