// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::thread
{

/// @brief Lock-free boolean flag primitive for spinlocks and single-event indicators.
class AtomicFlag final
{
private:
    Atomic<gp::UInt8> m_state{ 0u };

public:
    /// @brief Initializes the atomic flag to the cleared (false) state.
    [[nodiscard]] constexpr AtomicFlag() noexcept = default;

    /// @brief Deleted copy constructor.
    AtomicFlag(const AtomicFlag&) = delete;

    /// @brief Deleted copy assignment operator.
    AtomicFlag& operator=(const AtomicFlag&) = delete;

    /// @brief Deleted move constructor.
    AtomicFlag(AtomicFlag&&) = delete;

    /// @brief Deleted move assignment operator.
    AtomicFlag& operator=(AtomicFlag&&) = delete;

    /// @brief Destructor.
    constexpr ~AtomicFlag() noexcept = default;

    /// @brief Atomically sets the flag to true and returns the previous state.
    /// @param[in] order Memory ordering constraint.
    /// @return True if the flag was already set, false if it was previously clear.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        testAndSet(gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
    {
        return m_state.exchange(1u, order) != 0u;
    }

    /// @brief Atomically queries whether the flag is set without modifying it.
    /// @param[in] order Memory ordering constraint.
    /// @return Current state of the flag.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool
        test(gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) const noexcept
    {
        return m_state.load(order) != 0u;
    }

    /// @brief Atomically clears the flag to false.
    /// @param[in] order Memory ordering constraint (cannot be Acquire or AcqRel).
    GP_FORCEINLINE_HINT constexpr void clear(gp::memory::MemoryOrder order = gp::memory::MemoryOrder::SeqCst) noexcept
    {
        m_state.store(0u, order);
    }
};

}   // namespace gp::thread
