// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/IAllocator.hpp"
#include "memory/MemoryBase.hpp"

namespace gp::memory
{

/// @brief Lightweight 8-byte value-type allocator wrapper around a runtime `IAllocator*` interface pointer.
/// @details Enables type-erased containers (`gp::Vector<T, gp::memory::PolymorphicAllocator>`) to accept any
///          `IAllocator` implementation (`LinearAllocator`, `StackAllocator`, `PoolAllocator`, or the default heap
///          adapter) across subsystem or DLL boundaries without templating the API on the concrete arena type.
///          Default-constructed instances automatically bind to a process-wide `HeapAllocator`-backed `IAllocator`
///          singleton so `m_allocator` is never `nullptr`.
class GP_CORE_API PolymorphicAllocator
{
public:
    /// @brief Retrieves the process-wide default `IAllocator` backed by `HeapAllocator`.
    /// @return Reference to the static default `IAllocator` instance.
    [[nodiscard]] static IAllocator& getDefaultResource() noexcept;

private:
    IAllocator* m_allocator;

public:
    /// @brief Constructs a polymorphic allocator bound to `getDefaultResource()` (`HeapAllocator`).
    PolymorphicAllocator() noexcept
        : m_allocator(&getDefaultResource())
    {}

    /// @brief Constructs a polymorphic allocator bound to a concrete `IAllocator` reference.
    /// @param[in] allocator Reference to an active `IAllocator` (`LinearAllocator`, `StackAllocator`, `PoolAllocator`,
    /// etc.).
    constexpr explicit PolymorphicAllocator(IAllocator& allocator) noexcept
        : m_allocator(&allocator)
    {}

    /// @brief Constructs a polymorphic allocator from an `IAllocator*` pointer (falls back to `getDefaultResource()` if
    /// `nullptr`).
    /// @param[in] allocator Pointer to an `IAllocator` instance, or `nullptr`.
    explicit PolymorphicAllocator(IAllocator* allocator) noexcept
        : m_allocator(allocator != nullptr ? allocator : &getDefaultResource())
    {}

    /// @brief Copy constructor; copies the non-owning `IAllocator*` pointer.
    constexpr PolymorphicAllocator(const PolymorphicAllocator&) noexcept = default;

    /// @brief Copy assignment operator; copies the non-owning `IAllocator*` pointer.
    constexpr PolymorphicAllocator& operator=(const PolymorphicAllocator&) noexcept = default;

    /// @brief Move constructor; copies the non-owning `IAllocator*` pointer.
    constexpr PolymorphicAllocator(PolymorphicAllocator&&) noexcept = default;

    /// @brief Move assignment operator; copies the non-owning `IAllocator*` pointer.
    constexpr PolymorphicAllocator& operator=(PolymorphicAllocator&&) noexcept = default;

    /// @brief Destructor.
    constexpr ~PolymorphicAllocator() noexcept = default;

    /// @brief Allocates `sizeInBytes` aligned to `alignment` via the underlying `IAllocator`.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the allocated memory block, or `nullptr` on failure.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        return m_allocator->allocate(sizeInBytes, alignment);
    }

    /// @brief Resizes or relocates an existing allocation via the underlying `IAllocator`.
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
        return m_allocator->reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    /// @brief Deallocates `ptr` via the underlying `IAllocator`.
    /// @param[in] ptr Pointer to free (may be `nullptr`).
    /// @param[in] oldSizeInBytes Size in bytes of the allocation at `ptr`.
    /// @param[in] alignment Alignment used when allocating `ptr`.
    void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        m_allocator->deallocate(ptr, oldSizeInBytes, alignment);
    }

    /// @brief Queries the usable allocation size from the underlying `IAllocator`.
    /// @param[in] requestedSize Requested byte size.
    /// @param[in] alignment Requested byte alignment.
    /// @return Usable byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        return m_allocator->getActualAllocationSize(requestedSize, alignment);
    }

    /// @brief Queries whether `ptr` belongs to the underlying `IAllocator`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `m_allocator->owns(ptr)` returns true.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        return m_allocator->owns(ptr);
    }

    /// @brief Retrieves the debug name of the underlying `IAllocator`.
    /// @return Null-terminated static string name.
    [[nodiscard]] const char* getName() const noexcept
    {
        return m_allocator->getName();
    }

    /// @brief Retrieves the underlying `IAllocator*` resource pointer (guaranteed non-null).
    /// @return Pointer to the bound `IAllocator`.
    [[nodiscard]] constexpr IAllocator* getResource() const noexcept
    {
        return m_allocator;
    }

    /// @brief Compares two `PolymorphicAllocator` instances for equality of their underlying `IAllocator*` resource.
    /// @param[in] other Right-hand side `PolymorphicAllocator`.
    /// @return True if both wrap the exact same `IAllocator*` instance.
    [[nodiscard]] constexpr bool operator==(const PolymorphicAllocator& other) const noexcept
    {
        return m_allocator == other.m_allocator;
    }
};

static_assert(
    gp::concepts::IsAllocator<PolymorphicAllocator>, "PolymorphicAllocator must satisfy gp::concepts::IsAllocator."
);

}   // namespace gp::memory
