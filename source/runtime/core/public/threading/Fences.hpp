// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "platforms/base/PlatformAtomics.hpp"

namespace gp::thread
{

/// @brief Prevents compiler instruction reordering across this point without emitting CPU instructions.
GP_FORCEINLINE_HINT constexpr void compilerBarrier() noexcept
{
    if consteval
    {
        return;
    }
    else
    {
        gp::platform::Atomics::compilerBarrier();
    }
}

/// @brief Establishes memory synchronization between a thread and a signal handler on the same thread.
/// @param[in] order Memory ordering constraint to enforce.
GP_FORCEINLINE_HINT constexpr void atomicSignalFence(gp::memory::MemoryOrder order) noexcept
{
    if consteval
    {
        return;
    }
    else
    {
        gp::platform::Atomics::signalFence(order);
    }
}

/// @brief Issues a hardware memory barrier enforcing ordering constraints across CPU cores.
/// @param[in] order Memory ordering constraint to enforce.
GP_FORCEINLINE_HINT constexpr void atomicThreadFence(gp::memory::MemoryOrder order) noexcept
{
    if consteval
    {
        return;
    }
    else
    {
        gp::platform::Atomics::threadFence(order);
    }
}

}   // namespace gp::thread
