// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "macros/MacroUtilities.hpp"

namespace gp::memory
{

/// @brief Rounds a value up to the nearest multiple of the given alignment.
/// @details
/// Alignment must be a power of two. This is a branchless, constexpr operation suitable for hot-path usage.
/// @param[in] value The value to align upward.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return The smallest value >= input that is a multiple of alignment.
GP_FORCEINLINE constexpr USize alignUp(USize value, USize alignment) noexcept
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}

/// @brief Rounds a value down to the nearest multiple of the given alignment.
/// @param[in] value The value to align downward.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return The largest value <= input that is a multiple of alignment.
GP_FORCEINLINE constexpr USize alignDown(USize value, USize alignment) noexcept
{
    return value & ~(alignment - 1);
}

/// @brief Checks whether a value is aligned to the given boundary.
/// @param[in] value The value to check.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return True if value is a multiple of alignment.
GP_FORCEINLINE constexpr bool isAligned(USize value, USize alignment) noexcept
{
    return (value & (alignment - 1)) == 0;
}

/// @brief Checks whether a pointer is aligned to the given boundary.
/// @param[in] ptr The pointer to check.
/// @param[in] alignment The alignment boundary (must be a power of two).
/// @return True if the pointer address is a multiple of alignment.
GP_FORCEINLINE bool isAligned(const void* ptr, USize alignment) noexcept
{
    return isAligned(reinterpret_cast<USize>(ptr), alignment);
}

/// @brief Low-level cross-platform aligned memory allocation.
/// @details
/// This is the single point of entry for all raw allocation in the engine. On POSIX platforms, the size is rounded up
/// to a multiple of alignment to satisfy the aligned_alloc(3) contract. On Windows, _aligned_malloc has no such
/// constraint but we round anyway for consistency.
/// @param[in] size The number of bytes to allocate. Must be > 0.
/// @param[in] alignment The required alignment (must be a power of two).
/// @return Pointer to the allocated memory, or nullptr on failure.
GP_NODISCARD GP_FORCEINLINE void* malloc(USize size, USize alignment = alignof(gp::MaxAlign)) noexcept
{
    if (size == 0)
    {
        return nullptr;
    }
    const USize alignedSize = alignUp(size, alignment);
    return GP_ALIGNED_ALLOC(alignedSize, alignment);
}

/// @brief Low-level cross-platform aligned memory deallocation.
/// @details
/// Pairs with gp::memory::malloc. Passing nullptr is a safe no-op.
/// @param[in] ptr Pointer previously returned by gp::memory::malloc.
GP_FORCEINLINE void free(void* ptr) noexcept
{
    if (ptr)
    {
        GP_ALIGNED_FREE(ptr);
    }
}

/// @brief Compact metadata record stamped at the base of every allocation made through Allocator::construct /
///        Allocator::destruct and gp::MakeUnique.
/// @details
/// Solves the "polymorphic size problem": the deleter or destructor always receives the true sizeof(ConcreteType) from
/// the moment of construction, independent of the static type trough which the object is later accessed (e.g a
/// base-class pointer).
///
///
/// Memory layout produced by mallocWithHeader():
///
/// @code
/// ┌───────────────────────┬──────────────┬──────────────────────────────┐
/// │   AllocationHeader    │  back-offset │        T  object             │
/// │  size (8B) | align(8B)│   (USize,8B) │   sizeof(T), alignof(T)–pad  │
/// └───────────────────────┴──────────────┴──────────────────────────────┘
/// ^rawPtr                                ^objPtr  (the T* returned to callers)
/// │◄──────────── headerZone ────────────►│
/// @endcode
///
/// The **back-offset** (stored in the sizeof(USize) bytes immediately preceding objPtr) holds the byte distance from
/// rawPtr to objPtr. Given any objPtr, the back-offset is recovered at `*(USize*)(objPtr - sizeof(USize))`, which then
/// yields `rawPtr = objPtr - backOffset`.
///
/// ### Single-inheritance polymorphism
/// For a base class `B` and derived class `D` where `static_cast<B*>(d) == d` (same address), `B*` and `D*` point to
/// the same byte, so the back-offset read from `B* - sizeof(USize)` is identical to the one written for `D*`. Recovery
/// is exact. ✓
///
/// ### Multiple-inheritance limitation
/// When multiple inheritance causes a pointer adjustment (`static_cast<B*>(d) != d`), `B*` points into the interior of
/// the D allocation. `B* - sizeof(USize)` then reads data from inside the object, not from the back-offset slot,
/// producing a garbage rawPtr and near-certain heap corruption. **Do not store a UniquePtr<B> that was constructed as a
/// UniquePtr<D> under multiple inheritance.** The converting move constructor asserts on this condition in debug builds
/// (once GP_ASSERT is wired up).
struct AllocationHeader
{
    USize size;        //<! sizeof(ConcreteType) at the moment construct/makeUnique was called.
    USize alignment;   //<! alignof(ConcreteType) at the moment construct/makeUnique was called.
};

// Both fields are USize (typically 8 bytes each on 64-bit), so the struct is 16 bytes and naturally aligned to
// alignof(USize) == 8. The static_assert below enforces that assumption across all supported platforms.
static_assert(
    alignof(AllocationHeader) <= alignof(gp::MaxAlign),
    "AllocationHeader must be compatible with MaxAlign for proper alignment"
);

/// @brief Returns the size of the header zone, the region from rawPtr to objPtr.
/// @details
/// The zone contains the AllocationHeader (16 B) followed by the back-offset slot (sizeof(USize) B),
/// then padded up to the object's alignment so that objPtr satisfies alignof(T).
/// Computed entirely at compile time when 'alignment' is a constant.
/// @param[in] alignment The alignment of the object that will follow (alignof(T)).
/// @return Byte count from rawPtr to objPtr.
GP_FORCEINLINE constexpr USize headerZoneSize(USize alignment) noexcept
{
    return alignUp(sizeof(AllocationHeader) + sizeof(USize), alignment);
}

/// @brief Allocates memory for an object of the given size and alignment, prefixed by an AllocationHeader.
/// @details
/// Use this instead of gp::memory::malloc when the allocation will be managed by a type-erased deleter (DefaultDelete,
/// Allocator::destruct) that must recover the true concrete size at free time.
///
/// @param[in] size      sizeof(ConcreteType).
/// @param[in] alignment alignof(ConcreteType). Must be a power of two.
/// @return Pointer to the object storage (i.e. the address where placement-new should target),
///         or nullptr on allocation failure. The AllocationHeader lives headerZoneSize(alignment)
///         bytes before this pointer.
GP_NODISCARD GP_FORCEINLINE void* mallocWithHeader(USize size, USize alignment) noexcept
{
    // Clamp alignment so the header and back-offset slot are always correctly aligned.
    // AllocationHeader requires alignof(USize); sizeof(USize) == alignof(USize) on all GP targets.
    const USize effectiveAlign =
        (alignment < alignof(AllocationHeader)) ? static_cast<USize>(alignof(AllocationHeader)) : alignment;

    const USize zone = headerZoneSize(effectiveAlign);
    const USize total = zone + size;

    void* rawPtr = gp::memory::malloc(total, effectiveAlign);
    if (!rawPtr)
    {
        return nullptr;
    }

    char* const raw = static_cast<char*>(rawPtr);

    // Write AllocationHeader at the raw allocation base.
    AllocationHeader* const hdr = reinterpret_cast<AllocationHeader*>(raw);
    hdr->size = size;
    hdr->alignment = alignment;   // original, not clamped — preserves caller's intent

    // Write the back-offset in the sizeof(USize) bytes immediately before objPtr.
    // backOffset == zone, so given objPtr: rawPtr = (char*)objPtr - zone.
    USize* const backOffsetSlot = reinterpret_cast<USize*>(raw + zone - sizeof(USize));
    *backOffsetSlot = zone;

    return raw + zone;
}

/// @brief Frees memory previously allocated by mallocWithHeader and returns the stored header.
/// @details
/// Reads the back-offset from (objPtr - sizeof(USize)), recovers rawPtr, reads the AllocationHeader, then calls
/// gp::memory::free(rawPtr).
///
/// The caller is responsible for destroying the object before calling this function.
///
/// @param[in] objPtr Pointer to the object storage as returned by mallocWithHeader. Must not be nullptr (checked by the
///                   caller; no-op guard is in Allocator::destruct / DefaultDelete).
/// @return A copy of the AllocationHeader that was recorded at construction time.
GP_NODISCARD GP_FORCEINLINE AllocationHeader freeWithHeader(void* objPtr) noexcept
{
    const char* const obj = static_cast<const char*>(objPtr);

    // Recover the raw allocation base via the back-offset slot.
    const USize backOffset = *reinterpret_cast<const USize*>(obj - sizeof(USize));
    void* const rawPtr = const_cast<char*>(obj) - backOffset;

    // Capture header before freeing (header lives in the allocation we're about to release).
    const AllocationHeader hdr = *reinterpret_cast<const AllocationHeader*>(rawPtr);

    gp::memory::free(rawPtr);
    return hdr;
}

}   // namespace gp::memory
