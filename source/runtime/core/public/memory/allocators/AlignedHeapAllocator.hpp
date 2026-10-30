// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

/// @brief Stateless heap allocator enforcing a minimum compile-time byte alignment on all allocations.
/// @details Designed for SIMD vector containers (SSE 16-byte, AVX 32-byte) and cache-line-isolated buffers
///          (`HardwareConstants::cacheLineSize`). Satisfies `IsStatelessAllocator` so containers incur zero metadata
///          overhead when storing `AlignedHeapAllocator` via `[[no_unique_address]]`.
/// @tparam InAlignment Minimum power-of-two byte alignment enforced for every allocation (`>= kMinimumAlignment`).
template <gp::UInt32 InAlignment = kMinimumAlignment>
class AlignedHeapAllocator
{
    static_assert(
        InAlignment >= kMinimumAlignment && (InAlignment & (InAlignment - 1u)) == 0u,
        "AlignedHeapAllocator requires a power-of-two alignment >= kMinimumAlignment (8 bytes)."
    );

public:
    /// @brief Minimum alignment guaranteed by this allocator in bytes.
    static constexpr gp::UInt32 kGuaranteedAlignment = InAlignment;

    /// @brief Default constructor.
    constexpr AlignedHeapAllocator() noexcept = default;

    /// @brief Allocates a memory block aligned to at least `max(InAlignment, alignment)`.
    /// @param[in] sizeInBytes Number of bytes to allocate. Returns `nullptr` if `0`.
    /// @param[in] alignment Caller-requested alignment in bytes.
    /// @return Pointer to the aligned memory block, or `nullptr` if `sizeInBytes == 0` or allocation fails.
    [[nodiscard]] GP_FORCEINLINE_HINT void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        const gp::UInt32 effectiveAlignment = resolveAlignment(alignment);
        return detail::resolveActiveMalloc()->tryAllocate(sizeInBytes, effectiveAlignment);
    }

    /// @brief Resizes an existing aligned allocation while preserving at least `max(InAlignment, alignment)`.
    /// @param[in] ptr Existing memory block to resize, or `nullptr` to allocate a new block.
    /// @param[in] oldSizeInBytes Size of the existing allocation in bytes.
    /// @param[in] newSizeInBytes Requested new size in bytes. If `0`, frees `ptr` and returns `nullptr`.
    /// @param[in] alignment Caller-requested alignment in bytes.
    /// @return Pointer to the resized aligned memory block, or `nullptr` if freed or out of memory.
    [[nodiscard]] GP_FORCEINLINE_HINT void* reallocate(
        void* ptr,
        [[maybe_unused]] const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr == nullptr && newSizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        if (newSizeInBytes == 0u)
        {
            detail::resolveActiveMalloc()->deallocate(ptr);
            return nullptr;
        }

        const gp::UInt32 effectiveAlignment = resolveAlignment(alignment);
        return detail::resolveActiveMalloc()->tryReallocate(ptr, newSizeInBytes, effectiveAlignment);
    }

    /// @brief Deallocates a memory block previously obtained from `AlignedHeapAllocator`.
    /// @param[in] ptr Pointer to the memory block to free (safe no-op if `nullptr`).
    /// @param[in] oldSizeInBytes Size of the allocation in bytes.
    /// @param[in] alignment Alignment used during allocation.
    GP_FORCEINLINE_HINT void deallocate(
        void* ptr,
        [[maybe_unused]] const gp::USize oldSizeInBytes = 0u,
        [[maybe_unused]] const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        if (ptr != nullptr) [[likely]]
        {
            detail::resolveActiveMalloc()->deallocate(ptr);
        }
    }

    /// @brief Queries the underlying `Malloc` backend for the quantized block size at the enforced alignment.
    /// @param[in] requestedSize Minimum requested size in bytes.
    /// @param[in] alignment Caller-requested alignment in bytes.
    /// @return Quantized size in bytes (`>= requestedSize`).
    [[nodiscard]] GP_FORCEINLINE_HINT gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (requestedSize == 0u) [[unlikely]]
        {
            return 0u;
        }

        const gp::UInt32 effectiveAlignment = resolveAlignment(alignment);
        return detail::resolveActiveMalloc()->getActualAllocationSize(requestedSize, effectiveAlignment);
    }

    /// @brief Checks whether a pointer is non-null and aligned to `InAlignment`.
    /// @param[in] ptr Memory address to check.
    /// @return True if `ptr` is non-null and satisfies `InAlignment`; false otherwise.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool owns(const void* ptr) const noexcept
    {
        return ptr != nullptr && gp::memory::isAligned(ptr, static_cast<gp::UInt64>(InAlignment));
    }

    /// @brief Equality comparison; all stateless `AlignedHeapAllocator<InAlignment>` instances are interchangeable.
    /// @param[in] rhs Other `AlignedHeapAllocator` instance.
    /// @return Always true.
    [[nodiscard]] constexpr bool operator==([[maybe_unused]] const AlignedHeapAllocator& rhs) const noexcept
    {
        return true;
    }

private:
    /// @brief Computes `max(InAlignment, alignment)`.
    /// @param[in] alignment Caller-provided alignment.
    /// @return Effective power-of-two alignment in bytes.
    [[nodiscard]] static GP_FORCEINLINE_HINT constexpr gp::UInt32 resolveAlignment(const gp::UInt32 alignment) noexcept
    {
        return alignment > InAlignment ? alignment : InAlignment;
    }
};

static_assert(
    gp::concepts::IsStatelessAllocator<AlignedHeapAllocator<16u>>,
    "AlignedHeapAllocator<16> must satisfy gp::concepts::IsStatelessAllocator."
);

/// @brief 16-byte aligned heap allocator suitable for SSE / NEON 128-bit SIMD containers.
using Simd16HeapAllocator = AlignedHeapAllocator<16u>;

/// @brief 32-byte aligned heap allocator suitable for AVX / AVX2 256-bit SIMD containers.
using Simd32HeapAllocator = AlignedHeapAllocator<32u>;

/// @brief Cache-line aligned heap allocator preventing false sharing across CPU cores.
using CacheAlignedHeapAllocator = AlignedHeapAllocator<static_cast<gp::UInt32>(HardwareConstants::cacheLineSize)>;

}   // namespace gp::memory
