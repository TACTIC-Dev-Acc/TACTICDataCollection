#pragma once

#include <cstdint>
#include <format>
#include <string>

#include "PCH.h"

namespace Logging
{
    /// <summary>
    /// An address as an offset into SkyrimSE.exe, e.g. "SkyrimSE.exe+05BAB10": uppercase hex padded to 7 digits, exactly how CrashLogger writes it,
    /// so a hook's address in our log can be matched against a crash log's call stack by eye or by search.
    /// </summary>
    inline std::string GameOffset(std::uintptr_t address)
    {
        return std::format("SkyrimSE.exe+{:07X}", address - REL::Module::get().base());
    }
}
