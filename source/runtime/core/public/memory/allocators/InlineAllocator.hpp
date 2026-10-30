// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"
#include "memory/MemoryBase.hpp"
#include <utility>

namespace gp::memory
{

/// @brief Small-Buffer Optimization (SBO) hybrid allocator that serves allocations from an embedded inline buffer
///        and seamlessly spills over to `InFallbackAllocator` when capacity is exceeded.
/// @details Designed for containers such as `gp::Vector<T, gp::memory::InlineAllocator<256>>` or small strings/sets.
///          While total allocation fits inside `InCapacityBytes`, zero heap allocations occur. If a container grows
///          past `InCapacityBytes` during `reallocate()`, `InlineAllocator` allocates from `m_fallbackAllocator`,
///          copies the existing `oldSizeInBytes` payload out of `m_inlineBuffer`, and reclaims the inline buffer.
/// @tparam InCapacityBytes Size in bytes of the embedded inline storage (must be `> 0`).
/// @tparam InAlignment Alignment in bytes of the embedded inline storage (power of two; defaults to
/// `kMinimumAlignment`).
/// @tparam InFallbackAllocator Secondary allocator used when inline capacity is exceeded (defaults to `HeapAllocator`).
template <
    gp::USize InCapacityBytes,
    gp::UInt32 InAlignment = kMinimumAlignment,
    typename InFallbackAllocator = HeapAllocator>
class InlineAllocator
{
    static_assert(InCapacityBytes > 0u, "InlineAllocator capacity must be greater than zero.");
    static_assert(
        InAlignment >= kMinimumAlignment && gp::math::isPowerOfTwo(InAlignment),
        "InlineAllocator alignment must be a power of two >= kMinimumAlignment."
    );
    static_assert(
        gp::concepts::IsAllocator<InFallbackAllocator>,
        "InlineAllocator requires InFallbackAllocator to satisfy gp::concepts::IsAllocator."
    );

private:
    alignas(InAlignment) gp::UInt8 m_inlineBuffer[InCapacityBytes]{};
    gp::USize m_inlineOffset{ 0u };
    GP_NO_UNIQUE_ADDRESS InFallbackAllocator m_fallbackAllocator{};

public:
    using FallbackAllocatorType = InFallbackAllocator;
    static constexpr gp::USize kInlineCapacityBytes = InCapacityBytes;
    static constexpr gp::UInt32 kInlineAlignment = InAlignment;

public:
    /// @brief Default-constructs the inline allocator and its fallback allocator.
    constexpr InlineAllocator() noexcept = default;

    /// @brief Constructs the inline allocator with an explicit fallback allocator instance.
    /// @param[in] fallbackAllocator Fallback allocator to copy or move into this instance.
    constexpr explicit InlineAllocator(InFallbackAllocator fallbackAllocator) noexcept
        : m_fallbackAllocator(std::move(fallbackAllocator))
    {}

    /// @brief Copying or moving an `InlineAllocator` directly is disabled because active container pointers may point
    /// into `m_inlineBuffer`.
    InlineAllocator(const InlineAllocator&) = delete;
    InlineAllocator& operator=(const InlineAllocator&) = delete;
    InlineAllocator(InlineAllocator&&) = delete;
    InlineAllocator& operator=(InlineAllocator&&) = delete;

    /// @brief Destructor.
    constexpr ~InlineAllocator() noexcept = default;

    /// @brief Allocates `sizeInBytes` from the embedded inline buffer if space and alignment permit; otherwise
    /// delegates to `m_fallbackAllocator`.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the allocated memory block, or `nullptr` if `sizeInBytes == 0` or fallback fails.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
        GP_ASSERT(
            math::isPowerOfTwo(effectiveAlignment), "InlineAllocator::allocate requires a power-of-two alignment."
        );

        const gp::UInt64 baseAddress = reinterpret_cast<gp::UInt64>(m_inlineBuffer);
        const gp::UInt64 currentAddress = baseAddress + static_cast<gp::UInt64>(m_inlineOffset);
        const gp::UInt64 alignedAddress = memory::align(currentAddress, static_cast<gp::UInt64>(effectiveAlignment));
        const gp::USize padding = static_cast<gp::USize>(alignedAddress - currentAddress);
        const gp::USize totalRequired = padding + sizeInBytes;

        if (totalRequired <= (InCapacityBytes - m_inlineOffset))
        {
            m_inlineOffset += totalRequired;
            return reinterpret_cast<void*>(alignedAddress);
        }

        return m_fallbackAllocator.allocate(sizeInBytes, alignment);
    }

    /// @brief Resizes an allocation, seamlessly transitioning from inline storage to `m_fallbackAllocator` when growing
    /// past `InCapacityBytes`.
    /// @param[in] ptr Pointer previously returned by `allocate()` or `reallocate()` (or `nullptr`).
    /// @param[in] oldSizeInBytes Size in bytes of the existing allocation at `ptr`.
    /// @param[in] newSizeInBytes Desired new size in bytes.
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

        if (isInlinePointer(ptr))
        {
            const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
            const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_inlineBuffer);
            const gp::UIntPtr ptrAddress = reinterpret_cast<gp::UIntPtr>(ptr);
            const gp::USize ptrOffset = static_cast<gp::USize>(ptrAddress - baseAddress);
            const bool isAtInlineTop = (oldSizeInBytes > 0u) && ((ptrOffset + oldSizeInBytes) == m_inlineOffset);

            if (isAtInlineTop && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
            {
                if (newSizeInBytes <= oldSizeInBytes)
                {
                    m_inlineOffset = ptrOffset + newSizeInBytes;
                    return ptr;
                }

                const gp::USize additionalBytes = newSizeInBytes - oldSizeInBytes;
                if (additionalBytes <= (InCapacityBytes - m_inlineOffset))
                {
                    m_inlineOffset += additionalBytes;
                    return ptr;
                }
            }
            else if (
                newSizeInBytes <= oldSizeInBytes && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment))
            )
            {
                return ptr;
            }

            void* newPtr = m_fallbackAllocator.allocate(newSizeInBytes, alignment);
            if (newPtr != nullptr)
            {
                if (oldSizeInBytes > 0u)
                {
                    const gp::USize bytesToCopy = math::min(oldSizeInBytes, newSizeInBytes);
                    memory::copyMemory(newPtr, ptr, bytesToCopy);
                }

                if (isAtInlineTop)
                {
                    m_inlineOffset = ptrOffset;
                }
            }

            return newPtr;
        }

        return m_fallbackAllocator.reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    /// @brief Deallocates `ptr` by rewinding the inline cursor (if `ptr` is inside `m_inlineBuffer`) or forwarding to
    /// `m_fallbackAllocator`.
    /// @param[in] ptr Pointer to deallocate (may be `nullptr`).
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

        if (isInlinePointer(ptr))
        {
            const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_inlineBuffer);
            const gp::UIntPtr ptrAddress = reinterpret_cast<gp::UIntPtr>(ptr);
            const gp::USize ptrOffset = static_cast<gp::USize>(ptrAddress - baseAddress);

            if (oldSizeInBytes > 0u && (ptrOffset + oldSizeInBytes) == m_inlineOffset)
            {
                m_inlineOffset = ptrOffset;
            }
            else if (ptrOffset == 0u)
            {
                m_inlineOffset = 0u;
            }
            return;
        }

        m_fallbackAllocator.deallocate(ptr, oldSizeInBytes, alignment);
    }

    /// @brief Queries the usable size for an allocation of `requestedSize` and `alignment`.
    /// @param[in] requestedSize Requested byte size.
    /// @param[in] alignment Requested byte alignment.
    /// @return Actual usable byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (requestedSize == 0u)
        {
            return 0u;
        }

        const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
        if (requestedSize <= (InCapacityBytes - m_inlineOffset) && effectiveAlignment <= InAlignment)
        {
            return static_cast<gp::USize>(
                memory::align(static_cast<gp::UInt64>(requestedSize), static_cast<gp::UInt64>(effectiveAlignment))
            );
        }

        return m_fallbackAllocator.getActualAllocationSize(requestedSize, alignment);
    }

    /// @brief Checks whether `ptr` belongs to either the embedded inline buffer or `m_fallbackAllocator`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `isInlinePointer(ptr)` or `m_fallbackAllocator.owns(ptr)`.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        return isInlinePointer(ptr) || m_fallbackAllocator.owns(ptr);
    }

    /// @brief Checks whether `ptr` points directly inside the embedded `m_inlineBuffer`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` lies within `[m_inlineBuffer, m_inlineBuffer + InCapacityBytes)`.
    [[nodiscard]] bool isInlinePointer(const void* ptr) const noexcept
    {
        if (ptr == nullptr)
        {
            return false;
        }

        const gp::UIntPtr address = reinterpret_cast<gp::UIntPtr>(ptr);
        const gp::UIntPtr baseAddress = reinterpret_cast<gp::UIntPtr>(m_inlineBuffer);
        return address >= baseAddress && address < (baseAddress + InCapacityBytes);
    }

    /// @brief Resets the inline buffer cursor back to `0`.
    constexpr void resetInline() noexcept
    {
        m_inlineOffset = 0u;
    }

    /// @brief Retrieves the number of bytes currently used in the embedded inline buffer.
    /// @return Inline used bytes.
    [[nodiscard]] constexpr gp::USize getInlineUsedBytes() const noexcept
    {
        return m_inlineOffset;
    }

    /// @brief Retrieves the remaining unused bytes in the embedded inline buffer.
    /// @return Inline remaining bytes.
    [[nodiscard]] constexpr gp::USize getInlineRemainingBytes() const noexcept
    {
        return InCapacityBytes >= m_inlineOffset ? (InCapacityBytes - m_inlineOffset) : 0u;
    }

    /// @brief Retrieves a reference to the underlying fallback allocator.
    /// @return Reference to `m_fallbackAllocator`.
    [[nodiscard]] constexpr InFallbackAllocator& getFallbackAllocator() noexcept
    {
        return m_fallbackAllocator;
    }

    /// @brief Retrieves a const reference to the underlying fallback allocator.
    /// @return Const reference to `m_fallbackAllocator`.
    [[nodiscard]] constexpr const InFallbackAllocator& getFallbackAllocator() const noexcept
    {
        return m_fallbackAllocator;
    }
};

static_assert(
    gp::concepts::IsAllocator<InlineAllocator<256u>>, "InlineAllocator must satisfy gp::concepts::IsAllocator."
);

}   // namespace gp::memory
