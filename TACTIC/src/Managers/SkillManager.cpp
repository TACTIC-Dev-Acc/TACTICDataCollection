#include "Managers/SkillManager.h"

std::string_view CalculateSkillLevel(float skillValue)
{
    if (skillValue < 25.0f) return "low";
    if (skillValue < 50.0f) return "medium";
    if (skillValue < 75.0f) return "high";
    return "very high";
}

std::string_view CalculateCharacterLevel(std::uint16_t level)
{
    if (level < 5) return "very low";
    if (level < 15) return "low";
    if (level < 30) return "medium";
    if (level < 45) return "high";
    return "very high";
}
