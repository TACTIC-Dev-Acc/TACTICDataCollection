#include "Misc.h"

namespace RE
{
    /// Implementation of RE's DebugNotification function which is missing from the Clib repo this mod uses.
    void DebugNotification(const char* a_notification, const char* a_soundToPlay, bool a_cancelIfAlreadyQueued)
    {
        using func_t = decltype(&DebugNotification);
        REL::Relocation<func_t> func{Offset::DebugNotification};
        return func(a_notification, a_soundToPlay, a_cancelIfAlreadyQueued);
    }
}