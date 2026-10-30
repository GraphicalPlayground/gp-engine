// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/MemoryBase.hpp"
#include "profiling/Profiler.hpp"
#include <utility>

namespace gp::memory
{

/// @brief Profiler-instrumented decorator allocator that attributes every allocation, reallocation, and deallocation
///        to a named subsystem pool in Tracy (`GP_MEM_ALLOC_N`, `GP_MEM_FREE_N`, `GP_PLOT`) and maintains live
///        `AllocatorStats`.
/// @details Inspired by Decima, Anvil, and Unreal Engine LLM (Low-Level Memory Tracker) subsystem tagging. Can wrap
///          any `gp::concepts::IsAllocator` type (defaults to `HeapAllocator`).
/// @tparam InInnerAllocator Underlying allocator satisfying `gp::concepts::IsAllocator`.
template <typename InInnerAllocator = HeapAllocator>
class TaggedAllocator
{
    static_assert(
        gp::concepts::IsAllocator<InInnerAllocator>,
        "TaggedAllocator requires InInnerAllocator to satisfy gp::concepts::IsAllocator."
    );

public:
    using InnerAllocatorType = InInnerAllocator;

private:
    GP_NO_UNIQUE_ADDRESS InInnerAllocator m_innerAllocator{};
    const char* m_poolName{ "TaggedAllocator" };
    AllocatorStats m_stats{};

public:
    /// @brief Constructs a tagged allocator with the given static subsystem pool name.
    /// @param[in] poolName Static string literal identifying the subsystem pool in Tracy (e.g., `"Physics.Contacts"`).
    constexpr explicit TaggedAllocator(const char* poolName = "TaggedAllocator") noexcept
        : m_innerAllocator{}
        , m_poolName(poolName != nullptr ? poolName : "TaggedAllocator")
        , m_stats{}
    {}

    /// @brief Constructs a tagged allocator wrapping an explicit `innerAllocator` instance.
    /// @param[in] poolName Static string literal identifying the subsystem pool in Tracy.
    /// @param[in] innerAllocator Underlying allocator instance to move into this decorator.
    constexpr TaggedAllocator(const char* poolName, InInnerAllocator innerAllocator) noexcept
        : m_innerAllocator(std::move(innerAllocator))
        , m_poolName(poolName != nullptr ? poolName : "TaggedAllocator")
        , m_stats{}
    {}

    /// @brief Allocates `sizeInBytes` from `m_innerAllocator` and records the allocation under `m_poolName`.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the allocated memory block, or `nullptr` on failure.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        void* ptr = m_innerAllocator.allocate(sizeInBytes, alignment);
        if (ptr != nullptr)
        {
            m_stats.allocatedBytes += sizeInBytes;
            m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_stats.allocatedBytes);
            ++m_stats.activeAllocations;
            ++m_stats.totalAllocations;

            GP_MEM_ALLOC_N(ptr, sizeInBytes, m_poolName);
            GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
        }

        return ptr;
    }

    /// @brief Reallocates `ptr` via `m_innerAllocator` and updates profiler telemetry and byte counters.
    /// @param[in] ptr Existing memory block (or `nullptr`).
    /// @param[in] oldSizeInBytes Current byte size of `ptr`.
    /// @param[in] newSizeInBytes Desired new byte size.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the resized memory block, or `nullptr` if `newSizeInBytes == 0` or allocation fails.
    [[nodiscard]] void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (newSizeInBytes == 0u)
        {
            deallocate(ptr, oldSizeInBytes, alignment);
            return nullptr;
        }

        if (ptr == nullptr)
        {
            return allocate(newSizeInBytes, alignment);
        }

        GP_MEM_FREE_N(ptr, m_poolName);

        void* newPtr = m_innerAllocator.reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
        if (newPtr != nullptr)
        {
            const gp::USize subtracted = math::min(m_stats.allocatedBytes, oldSizeInBytes);
            m_stats.allocatedBytes = (m_stats.allocatedBytes - subtracted) + newSizeInBytes;
            m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_stats.allocatedBytes);
            ++m_stats.totalAllocations;

            GP_MEM_ALLOC_N(newPtr, newSizeInBytes, m_poolName);
            GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
        }
        else
        {
            GP_MEM_ALLOC_N(ptr, oldSizeInBytes, m_poolName);
        }

        return newPtr;
    }

    /// @brief Deallocates `ptr` via `m_innerAllocator` and records the deallocation under `m_poolName`.
    /// @param[in] ptr Pointer to free (may be `nullptr`).
    /// @param[in] oldSizeInBytes Size in bytes of the allocation at `ptr`.
    /// @param[in] alignment Alignment used when allocating `ptr`.
    void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr == nullptr)
        {
            return;
        }

        GP_MEM_FREE_N(ptr, m_poolName);
        m_innerAllocator.deallocate(ptr, oldSizeInBytes, alignment);

        const gp::USize subtracted = math::min(m_stats.allocatedBytes, oldSizeInBytes);
        m_stats.allocatedBytes -= subtracted;
        if (m_stats.activeAllocations > 0ull)
        {
            --m_stats.activeAllocations;
        }
        ++m_stats.totalDeallocations;

        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
    }

    /// @brief Queries the usable allocation size from `m_innerAllocator`.
    /// @param[in] requestedSize Requested byte size.
    /// @param[in] alignment Requested byte alignment.
    /// @return Usable byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        return m_innerAllocator.getActualAllocationSize(requestedSize, alignment);
    }

    /// @brief Queries whether `ptr` belongs to `m_innerAllocator`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `m_innerAllocator.owns(ptr)` returns true.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        return m_innerAllocator.owns(ptr);
    }

    /// @brief Retrieves the static subsystem pool name associated with this tagged allocator.
    /// @return Null-terminated static string name.
    [[nodiscard]] constexpr const char* getName() const noexcept
    {
        return m_poolName;
    }

    /// @brief Retrieves the current telemetry statistics accumulated by this tagged allocator.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Retrieves a reference to the wrapped inner allocator.
    /// @return Reference to `m_innerAllocator`.
    [[nodiscard]] constexpr InInnerAllocator& getInnerAllocator() noexcept
    {
        return m_innerAllocator;
    }

    /// @brief Retrieves a const reference to the wrapped inner allocator.
    /// @return Const reference to `m_innerAllocator`.
    [[nodiscard]] constexpr const InInnerAllocator& getInnerAllocator() const noexcept
    {
        return m_innerAllocator;
    }
};

static_assert(
    gp::concepts::IsAllocator<TaggedAllocator<HeapAllocator>>,
    "TaggedAllocator<HeapAllocator> must satisfy gp::concepts::IsAllocator."
);

}   // namespace gp::memory
