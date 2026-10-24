// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::memory
{

/// @brief Ordering constraints that an atomic operation or fence places on surrounding memory accesses.
/// @details The values match the C++ memory model. Each platform backend translates them into compiler fences and,
///          where the hardware needs it, CPU barrier instructions.
enum class MemoryOrder : gp::UInt8
{
    /// @brief The operation is atomic but imposes no ordering on other reads or writes.
    Relaxed = 0,

    /// @brief Data-dependent load ordering. Every supported compiler treats it as Acquire.
    Consume = 1,

    /// @brief For loads: later reads and writes cannot be reordered before this load.
    Acquire = 2,

    /// @brief For stores: earlier reads and writes cannot be reordered after this store.
    Release = 3,

    /// @brief For read-modify-write operations: Acquire and Release at the same time.
    AcqRel = 4,

    /// @brief AcqRel plus a single total order of all SeqCst operations that every thread observes.
    SeqCst = 5
};

}   // namespace gp::memory

namespace gp
{

using memory::MemoryOrder;

}   // namespace gp
