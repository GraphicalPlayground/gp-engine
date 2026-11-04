// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp
{

/// @brief Selects the synchronization strategy used by intrusive reference counters.
/// @details Allows subsystems to opt into zero-overhead non-atomic reference counting when an object's lifetime is
///          strictly confined to a single thread, while defaulting to hardware lock-free atomics for cross-thread
///          resources such as RHI buffers, textures, and pipeline states.
enum class RefCountThreadSafety : gp::UInt8
{
    /// @brief Uses plain integer increments and decrements with zero atomic bus lock overhead.
    /// @note Must only be used when all `RefCountedPtr` copies and resets occur on a single thread.
    SingleThreaded = 0,

    /// @brief Uses lock-free `AtomicRefStateCounter` operations (`Relaxed` increment, `AcqRel` decrement).
    /// @note Safe for concurrent sharing across the Game Thread, Render Thread, RHI Thread, and worker jobs.
    ThreadSafe = 1
};

/// @brief Bit-packed lifecycle state flags stored in the upper 2 bits of a 32-bit `AtomicRefStateCounter`.
/// @details Inspired by Unreal Engine 5 (`FRHIResource::FAtomicFlags`) and Frostbite/Decima RHI retirement queues.
///          Packing these flags into the same 32-bit atomic word as the 30-bit reference count (`[0..29]`) enables
///          single-instruction lock-free state transitions during multi-threaded deferred GPU deletion.
enum class RefStateFlags : gp::UInt32
{
    /// @brief Standard live state; no pending deletion or active destruction flags are set.
    None = 0x00000000u,

    /// @brief Bit 30 (`0x40000000`): Object has reached zero references and is queued in a deferred deletion list.
    /// @details Prevents duplicate enqueueing if a transient lookup briefly resurrects and releases the resource
    ///          before the GPU fence retires.
    MarkedForDelete = 1u << 30u,

    /// @brief Bit 31 (`0x80000000`): Terminal flag latched immediately before physical destructor execution begins.
    /// @details Any attempt to increment the reference count while `Deleting` is set triggers a fatal assertion.
    Deleting = 1u << 31u
};

}   // namespace gp

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::RefStateFlags);
