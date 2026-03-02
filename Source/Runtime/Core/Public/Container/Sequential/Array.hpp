// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Container/Utility/Iterator.hpp"
#include "Container/Utility/ReverseIterator.hpp"
#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"

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
    using Storage = GP_ALIGN(alignof(T)) Byte[sizeof(T) * (InlineCapacity > 0 ? InlineCapacity : 1)];

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

public:
    static constexpr SizeT kInlineCapacity = InlineCapacity;
    static constexpr SizeT kMinHeapCapacity = 16;
};

}   // namespace GP

#include "Container/Sequential/Array.inl"
