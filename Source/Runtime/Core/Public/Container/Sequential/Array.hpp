// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "CoreTypes.hpp"
#include "Memory/Allocators/DefaultAllocator.hpp"
#include "Memory/Allocators/IAllocator.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/InitializerList.hpp"

namespace GP
{

/// @brief Dynamic resizable array with Small Buffer Optimization (SBO).
///
/// Stores up to @p InlineCapacity elements inline (stack / in-object storage) before spilling to the heap. Growth
/// factor of 2× gives O(1) amortised PushBack. Trivially copyable types use memcpy for all bulk operations.
///
/// @tparam T
/// @tparam InlineCapacity
template <typename T, SizeT InlineCapacity = 8>
class TArray
{
private:
    static_assert(!CReference<T>, "TArray cannot store reference types");
    static_assert(CDestructible<T>, "TArray element type must be destructible");

private:
    /// @brief Empty storage type used when InlineCapacity is 0 to avoid defining the inline storage member, which would
    ///        be wasteful and potentially cause issues with zero-sized arrays.
    struct EmptyStorage
    {};

    /// @brief Storage for inline elements when InlineCapacity > 0. Uses TConditional_T to avoid defining this member
    ///        when InlineCapacity is 0, which would be wasteful and potentially cause issues with zero-sized arrays.
    struct GP_ALIGN(alignof(T)) Storage
    {
        Byte data[sizeof(T) * (InlineCapacity > 0 ? InlineCapacity : 1)];
    };

public:
    using ValueType = T;
    using SizeType = SizeT;
    using DiffType = SSizeT;
    using Pointer = T*;
    using ConstPointer = const T*;
    using Reference = T&;
    using ConstReference = const T&;
    using Iterator = TIterator<T>;
    using ConstIterator = TConstIterator<T>;
    using ReverseIterator = TReverseIteratorT<T>;
    using ConstReverseIterator = TConstReverseIterator<T>;

private:
    IAllocator* m_alloc{ &GetDefaultAllocator() };   //<! Allocator used for heap operations.
    Pointer m_data{ nullptr };                       //<! Pointer to the array data (either inline or heap).
    SizeT m_size{ 0 };                               //<! Current number of elements in the array.
    SizeT m_capacity{ InlineCapacity };              //<! Current capacity of the array.
    SizeT m_heapBytes{ 0 };                          //<! Number of bytes currently allocated on the heap.

    /// @brief Storage for inline elements when InlineCapacity > 0. Uses TConditional_T to avoid defining this member
    ///        when InlineCapacity is 0, which would be wasteful and potentially cause issues with zero-sized arrays.
    GP_NO_UNIQUE_ADDRESS TConditional_T<(InlineCapacity > 0), Storage, EmptyStorage> m_inline;

public:
    static constexpr SizeT kInlineCapacity = InlineCapacity;
    static constexpr SizeT kMinHeapCapacity = 16;

public:
    /// @brief
    /// @param alloc
    explicit TArray(IAllocator& alloc = GetDefaultAllocator()) noexcept
        : m_alloc(&alloc)
    {
        // TODO: Add SetupInline() implementation.
    }

    /// @brief
    /// @param count
    /// @param alloc
    explicit TArray(SizeT count, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        // TODO: Add SetupInline() and Resize() implementations.
    }

    /// @brief
    /// @param count
    /// @param value
    /// @param alloc
    TArray(SizeT count, const T& value, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        // TODO: Add SetupInline() and Resize() implementations.
    }

    /// @brief
    /// @param list
    /// @param alloc
    TArray(TInitializerList<T> list, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        // TODO: Add SetupInline() and Resver(list.size()) and EmplaceBack() implementations.
    }

    /// @brief
    /// @param first
    /// @param last
    /// @param alloc
    TArray(const T* first, const T* last, IAllocator& alloc = GetDefaultAllocator())
        : m_alloc(&alloc)
    {
        // TODO: Add SetupInline() and Resize() implementations.
    }

    /// @brief
    /// @param other
    TArray(const TArray& other)
        : m_alloc(other.m_alloc)
    {
        // TODO: Add SetupInline() and CopyFrom() implementations.
    }

    /// @brief
    /// @param other
    TArray(TArray&& other) noexcept
        : m_alloc(other.m_alloc)
    {
        // TODO: Add SetupInline() and MoveFrom(GP::Move()) implementations.
    }

public:
    GP_NODISCARD constexpr SizeType Size() const noexcept { return m_size; }

    GP_NODISCARD constexpr SizeType Capacity() const noexcept { return m_capacity; }

    GP_NODISCARD constexpr bool IsEmpty() const noexcept { return m_size == 0; }
};

}   // namespace GP

#include "Container/Sequential/Array.inl"
