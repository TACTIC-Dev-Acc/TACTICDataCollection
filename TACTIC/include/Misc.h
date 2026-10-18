#pragma once

#include <Offsets.h>

namespace RE
{
    /// <summary>
    /// Implementaion of missing DebugNotification.
    /// </summary>
    /// <param name="a_notification"></param>
    /// <param name="a_soundToPlay"></param>
    /// <param name="a_cancelIfAlreadyQueued"></param>
    void DebugNotification(const char* a_notification, const char* a_soundToPlay = 0, bool a_cancelIfAlreadyQueued = true);
}