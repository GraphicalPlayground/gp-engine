// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief Lightweight checkpoint capturing the state of a linear or stack arena at a specific point in time.
/// @details Used with `LinearAllocator`, `StackAllocator`, `FixedBufferAllocator`, and `ScopedMemoryMark` to unwind
///          transient allocations in O(1) time without freeing blocks individually.
struct MemoryMark
{
    /// @brief Byte offset from the base of the arena buffer at the time the mark was taken.
    gp::USize offset{ 0u };

    /// @brief Number of active allocations in the arena when the mark was recorded.
    gp::UInt64 activeAllocations{ 0ull };

    /// @brief Cumulative bytes tracked as allocated when the mark was recorded.
    gp::USize allocatedBytes{ 0u };
};

/// @brief Telemetry snapshot reporting live and historical memory consumption for an allocator or arena.
struct AllocatorStats
{
    /// @brief Current number of bytes actively allocated.
    gp::USize allocatedBytes{ 0u };

    /// @brief Historical high-water mark (maximum simultaneous allocated bytes) observed since creation or reset.
    gp::USize peakAllocatedBytes{ 0u };

    /// @brief Total byte capacity of the underlying arena buffer, or 0 for unbounded heap allocators.
    gp::USize capacityBytes{ 0u };

    /// @brief Number of currently live allocations not yet freed or rewound.
    gp::UInt64 activeAllocations{ 0ull };

    /// @brief Cumulative count of successful allocation requests over the allocator's lifetime.
    gp::UInt64 totalAllocations{ 0ull };

    /// @brief Cumulative count of deallocation requests over the allocator's lifetime.
    gp::UInt64 totalDeallocations{ 0ull };
};

/// @brief RAII scope guard that automatically rewinds a markable arena allocator to its saved state upon scope exit.
/// @details Models the AAA frame/task marker pattern (e.g., id Tech / Unreal `FMemMark` / Decima scratch markers).
///          All allocations made from the bound arena within the C++ lexical scope are reclaimed in a single O(1)
///          cursor rewind when `ScopedMemoryMark` is destroyed.
/// @tparam InArenaAllocator Type of the arena allocator (must provide `mark()` and `rewind(const MemoryMark&)`).
template <typename InArenaAllocator>
class ScopedMemoryMark final
{
private:
    InArenaAllocator& m_allocator;
    MemoryMark m_mark{};

public:
    /// @brief Captures the current cursor mark of the specified arena allocator.
    /// @param[in,out] allocator Reference to the arena allocator to checkpoint and rewind on destruction.
    explicit constexpr ScopedMemoryMark(InArenaAllocator& allocator) noexcept
        : m_allocator(allocator)
        , m_mark(allocator.mark())
    {}

    /// @brief Copying or moving a scoped memory mark is forbidden to preserve strict LIFO lexical ordering.
    ScopedMemoryMark(const ScopedMemoryMark&) = delete;
    ScopedMemoryMark& operator=(const ScopedMemoryMark&) = delete;
    ScopedMemoryMark(ScopedMemoryMark&&) = delete;
    ScopedMemoryMark& operator=(ScopedMemoryMark&&) = delete;

    /// @brief Destructor that rewinds the bound arena allocator back to the captured checkpoint.
    constexpr ~ScopedMemoryMark() noexcept
    {
        m_allocator.rewind(m_mark);
    }

    /// @brief Manually rewinds the arena to the saved checkpoint before the scope exits.
    /// @note Subsequent allocations within the same scope will also be rewound when the destructor runs.
    constexpr void rewindNow() noexcept
    {
        m_allocator.rewind(m_mark);
    }

    /// @brief Retrieves the saved memory checkpoint.
    /// @return Const reference to the captured `MemoryMark`.
    [[nodiscard]] constexpr const MemoryMark& getMark() const noexcept
    {
        return m_mark;
    }
};

}   // namespace gp::memory
