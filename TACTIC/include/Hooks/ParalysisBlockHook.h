#pragma once

#include "PCH.h"

#include <string>
#include <unordered_set>

/// <summary>
/// Disables all paralysis effects.
/// </summary>
class ParalysisBlockHook
{
public:
    /// <summary>
    /// Loads the allowlisted Paralysis effects.
    /// </summary>
    /// <param name="path">Path to the JSON file.</param>
    /// <returns>True if the file was succesfully opened. False otherwise.</returns>
    static bool LoadAllowlist(const std::string& path);

    /// <summary>
    /// Installs the hook on the NPC and player MagicTarget::AddTarget.
    /// </summary>
    static void Install();

    /// <summary>
    /// How many paralysis applications have been blocked since the game was launched. Useful for debugging.
    /// </summary>
    static std::uint64_t GetBlockedCount();

private:
    static bool ShouldBlock(RE::MagicTarget* a_this, const RE::MagicTarget::AddTargetData& a_data);

    static bool CharacterAddTarget(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data);
    static bool PlayerAddTarget(RE::MagicTarget* a_this, RE::MagicTarget::AddTargetData& a_data);

    static inline REL::Relocation<decltype(CharacterAddTarget)> _characterAddTarget;
    static inline REL::Relocation<decltype(PlayerAddTarget)> _playerAddTarget;

    // FormIDs of the allowlisted magic effects
    static inline std::unordered_set<RE::FormID> _allowedEffects;
};
