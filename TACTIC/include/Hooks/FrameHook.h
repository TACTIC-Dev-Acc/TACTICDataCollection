#pragma once
#include "PCH.h"

class FrameHook 
{
public:
    /// <summary>
    /// Installs the FrameHook
    /// </summary>
    static void Install();

private:
    /// <summary>
    /// Implements and extends the OnFrameUpdate function
    /// </summary>
    static void OnFrameUpdate();
    inline static REL::Relocation<decltype(OnFrameUpdate)> _originalFrameUpdate;
};