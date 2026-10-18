#pragma once

#include <RE/Skyrim.h>

/// <summary>
/// Implementation for missing IsPowerAttacking.
/// </summary>
/// <param name="actor"></param>
/// <returns></returns>
inline bool IsPowerAttacking(RE::Actor* actor)
{
    using func_t = bool(RE::Actor*);

    static REL::Relocation<func_t> func{RELOCATION_ID(37639, 38592)};

    return actor && func(actor);
}
