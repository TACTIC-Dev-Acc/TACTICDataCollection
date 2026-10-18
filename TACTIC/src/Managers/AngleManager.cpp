#include "Managers/AngleManager.h"

// Use broad categories for easier training
std::string_view AngleToString(float angle)
{
    if (angle >= -45.0f && angle <= 45.0f) return "front";
    if (angle > 45.0f) return "right";
    return "left";
}

float GetAngle(RE::Actor* sourceActor, RE::Actor* targetActor) 
{
    if (!sourceActor || !targetActor) return 0.0f;

    return sourceActor->GetHeadingAngle(targetActor->GetPosition(), false);
}

std::string_view GetAngleRepresentation(RE::Actor* sourceActor, RE::Actor* targetActor) 
{
    return AngleToString(GetAngle(sourceActor, targetActor));
}

float GetCameraRelativeAngle(const RE::NiPoint3& a_targetPos)
{
    auto* camera = RE::PlayerCamera::GetSingleton();
    if (!camera or !camera->cameraRoot) return 0.0f;

    auto* self = RE::PlayerCharacter::GetSingleton();
    if (!self) return 0.0f;

    RE::NiTransform cameraTransform;
    cameraTransform.rotate = camera->cameraRoot->world.rotate;
    cameraTransform.translate = self->GetPosition();

    return cameraTransform.GetHeadingAngle(a_targetPos);
}
