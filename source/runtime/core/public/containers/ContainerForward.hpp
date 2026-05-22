// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::container
{

/// @section Details forward declarations

template <typename T, bool AllowDuplicateKeys = false>
struct DefaultKeyFunctions;
template <typename KeyType, typename ValueType, bool AllowDuplicateKeys>
struct DefaultMapHashtableKeyFunctions;
template <typename T = void>
struct Less;

/// @section String related forward declarations

template <concepts::IsCharacter CharType>
class BasicStringView;

}   // namespace gp::container

namespace gp
{

// TODO: Move this out of there
template <typename>
struct TypeTraits;

/// @section Array related forward declarations

template <typename T, USize N, USize Alignment = alignof(T)>
class Array;

/// @section Vector related forward declarations

template <typename T, typename Allocator /* = gp::memory::DefaultAllocator */>
class Vector;
template <typename T>
using Vector64 = Vector<T, float /* TODO: Replace with gp::memory::DefaultAllocator64 */>;
template <typename T, typename SizeType = Int32>
class VectorView;
template <typename T>
using VectorView64 = VectorView<T, Int64>;
template <typename T, typename SizeType = Int32>
using ConstVectorView = VectorView<const T, SizeType>;
template <typename T>
using ConstVectorView64 = VectorView<const T, Int64>;

/// @section Map related forward declarations

template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultSparseSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, false>>
class SparseMap;
template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultCompactSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, false>>
class CompactMap;
template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, false>>
class Map;
template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultSparseSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, true>>
class SparseMultiMap;
template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultCompactSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, true>>
class CompactMultiMap;
template <
    typename KeyType,
    typename ValueType,
    typename SetAllocator /* = DefaultSetAllocator */,
    typename KeyFunctions = container::DefaultMapHashtableKeyFunctions<KeyType, ValueType, true>>
class MultiMap;

template <
    typename KeyType,
    typename ValueType,
    typename ArrayAllocator /* = FDefaultAllocator */,
    typename SortPredicate = container::Less<typename TypeTraits<KeyType>::ConstPointerType>>
class SortedMap;
template <typename T, typename ArrayAllocator /* = FDefaultAllocator */, typename SortPredicate = container::Less<T>>
class SortedSet;
template <
    typename T,
    typename KeyFunctions = container::DefaultKeyFunctions<T>,
    typename Allocator = float /* = DefaultSparseSetAllocator */>
class SparseSet;
template <
    typename T,
    typename KeyFunctions = container::DefaultKeyFunctions<T>,
    typename Allocator = float /* = DefaultCompactSetAllocator */>
class CompactSet;
template <
    typename T,
    typename KeyFunctions = container::DefaultKeyFunctions<T>,
    typename Allocator = float /* = DefaultSetAllocator */>
class Set;
template <typename T, typename InSizeType = Int32>
class StridedView;
template <typename T, typename SizeType = Int32>
using ConstStridedView = StridedView<const T, SizeType>;

}   // namespace gp
