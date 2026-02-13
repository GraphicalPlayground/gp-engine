// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <cstdlib>

namespace GP::Memory
{

/// \brief Default heap allocator. Wraps the platform's aligned allocation primitives.
///
/// This is the allocator every container defaults to when no custom allocator is provided.
/// It delegates to platform-specific aligned allocation:
///   - Windows: _aligned_malloc / _aligned_free
///   - POSIX:   aligned_alloc / free (C++17 std::aligned_alloc)
///
/// \note Unlike std::allocator, this allocator is not typed at the allocation level, Allocate() returns void*. The
/// caller placement-news the object into the returned memory. This design (used by EASTL, Frostbite, Unreal) separates
/// memory policy from object construction, allowing a single allocator instance to serve multiple types.
///
/// The template parameter T is used only for default alignment (alignof(T)) and to satisfy
/// the IsAllocator concept. The allocator itself stores no per-T state.
///
/// \tparam T The type this allocator is nominally associated with. Used for default alignment.
template <typename T>
class TAllocator
{
public:
    using ValueType = T;      //<! Alias for the type this allocator is associated with.
    using SizeType = SizeT;   //<! Alias for size type, typically std::size_t.

public:
    /// \brief Default constructor. No state is initialized since this allocator is stateless.
    constexpr TAllocator() noexcept = default;

    /// \brief Copy constructor. Defaulted since this allocator is stateless and trivially copyable.
    constexpr TAllocator(const TAllocator&) noexcept = default;

    /// \brief Destructor. Defaulted since this allocator is stateless and has no resources to release.
    constexpr ~TAllocator() noexcept = default;

    /// \brief Allow construction from an allocator of a different type. This enables containers to rebind allocators
    /// for internal node types (e.g., TList internally allocates nodes, not T directly).
    /// \tparam U The type of the other allocator. This parameter is unused since the allocator is stateless.
    template <typename U>
    constexpr TAllocator(const TAllocator<U>&) noexcept
    {}

public:
    /// \brief Copy assignment operator. Defaulted since this allocator is stateless and trivially copyable.
    constexpr TAllocator& operator=(const TAllocator&) noexcept = default;

    /// \brief Comparisons, all default heap allocators are interchangeable.
    /// \tparam U The type of the other allocator. This parameter is unused since all TAllocator<U> are interchangeable.
    /// \return true if the allocators are considered equal (always true for TAllocator), false otherwise.
    template <typename U>
    GP_NODISCARD constexpr bool operator==(const TAllocator<U>&) const noexcept
    {
        return true;
    }

    /// \brief Comparisons, all default heap allocators are interchangeable.
    /// \tparam U The type of the other allocator. This parameter is unused since all TAllocator<U> are interchangeable.
    /// \return true if the allocators are considered not equal (always false for TAllocator since they are
    /// interchangeable), false otherwise.
    template <typename U>
    GP_NODISCARD constexpr bool operator!=(const TAllocator<U>&) const noexcept
    {
        return false;
    }

public:
    /// \brief Allocate a block of memory with specified size and alignment.
    /// \param size Number of bytes to allocate. Must be > 0.
    /// \param align Alignment requirement in bytes. Must be a power of two. Defaults to alignof(T).
    /// \return Pointer to the allocated memory block, or nullptr on failure.
    /// \note: We use platform-specific aligned allocation rather than over-allocating + manual alignment because:
    /// 1. The OS can satisfy alignment natively with no wasted bytes
    /// 2. Large alignments (e.g., 64-byte cache lines, 4096-byte pages) work correctly
    /// 3. The address returned can be passed directly to the corresponding Free
    GP_NODISCARD void* Allocate(SizeType size, SizeType align = alignof(T))
    {
        GP_ASSERT(size > 0, "Cannot allocate zero bytes");
        GP_ASSERT((align & (align - 1)) == 0, "Alignment must be a power of two");

        // Ensure alignment is at least the platform minimum
        if (align < alignof(std::max_align_t)) { align = alignof(std::max_align_t); }

#if GP_PLATFORM_WINDOWS
        void* ptr = ::_aligned_malloc(size, align);
#else
        // C11 / C++17 aligned_alloc requires size to be a multiple of alignment.
        SizeType alignedSize = (size + align - 1) & ~(align - 1);
        void* ptr = ::aligned_alloc(align, alignedSize);
#endif

        GP_ASSERT(ptr != nullptr, "Allocation failed: size=%zu, align=%zu", size, align);
        return ptr;
    }

    /// \brief Deallocate a block previously returned by Allocate().
    /// \param ptr Pointer to the memory block. May be nullptr (no-op).
    void Deallocate(void* ptr)
    {
        if (ptr == nullptr) { return; }

#if GP_PLATFORM_WINDOWS
        ::_aligned_free(ptr);
#else
        ::free(ptr);
#endif
    }

    /// \brief Allocate memory for N objects of type T. Convenience wrapper that computes size = count * sizeof(T).
    /// \param count Number of objects to allocate. Must be > 0.
    /// \return Pointer to the allocated memory block, or nullptr on failure.
    GP_NODISCARD T* AllocateTyped(SizeType count) { return static_cast<T*>(Allocate(count * sizeof(T), alignof(T))); }
};

}   // namespace GP::Memory
