// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

namespace gp::container
{

/// @section Details forward declarations

template <typename InT, bool InAllowDuplicateKeys = false>
struct DefaultKeyFunctions;
template <typename InKeyType, typename InValueType, bool InAllowDuplicateKeys>
struct DefaultMapHashtableKeyFunctions;
template <typename InT = void>
struct Less;

/// @section String related forward declarations

enum class SearchCase : gp::UInt8;
enum class SearchDir : gp::UInt8;
enum class StringSplitOptions : gp::UInt8;
enum class NameFindMode : gp::UInt8;

using NameEntryId = gp::UInt32;
struct NameEntryHeader;
class NameHash;
class NamePool;
class Name;

template <concepts::IsCharacter InCharT>
struct CharTraits;

template <concepts::IsCharacter InCharT>
class BasicStringView;

template <concepts::IsCharacter InCharT, gp::USize InN>
class BasicFixedString;

template <concepts::IsCharacter InCharT, typename InAllocator = gp::memory::DefaultAllocator>
class BasicString;

}   // namespace gp::container

namespace gp
{

// TODO: Move this out of there
template <typename>
struct TypeTraits;

/// @section String aliases forward declarations

using StringView = container::BasicStringView<char>;
using WStringView = container::BasicStringView<wchar_t>;
using U8StringView = container::BasicStringView<char8_t>;
using U16StringView = container::BasicStringView<char16_t>;
using U32StringView = container::BasicStringView<char32_t>;

template <gp::USize InN>
using FixedString = container::BasicFixedString<char, InN>;
template <gp::USize InN>
using FixedWString = container::BasicFixedString<wchar_t, InN>;
template <gp::USize InN>
using FixedU8String = container::BasicFixedString<char8_t, InN>;
template <gp::USize InN>
using FixedU16String = container::BasicFixedString<char16_t, InN>;
template <gp::USize InN>
using FixedU32String = container::BasicFixedString<char32_t, InN>;

using String = container::BasicString<char, gp::memory::DefaultAllocator>;
using WString = container::BasicString<wchar_t, gp::memory::DefaultAllocator>;
using U8String = container::BasicString<char8_t, gp::memory::DefaultAllocator>;
using U16String = container::BasicString<char16_t, gp::memory::DefaultAllocator>;
using U32String = container::BasicString<char32_t, gp::memory::DefaultAllocator>;

using ScratchString = container::BasicString<char, gp::memory::ThreadScratchAllocator>;
using PolymorphicString = container::BasicString<char, gp::memory::PolymorphicAllocator>;

template <
    gp::USize InCapacityBytes,
    gp::UInt32 InAlignment = 8u,
    typename InFallbackAllocator = gp::memory::DefaultAllocator>
using InlineString =
    container::BasicString<char, gp::memory::InlineAllocator<InCapacityBytes, InAlignment, InFallbackAllocator>>;

using NameFindMode = container::NameFindMode;
using NameEntryId = container::NameEntryId;
using NameEntryHeader = container::NameEntryHeader;
using NameHash = container::NameHash;
using NamePool = container::NamePool;
using Name = container::Name;

/// @section Array related forward declarations

template <typename InT, USize InN, USize InAlignment = alignof(InT)>
class Array;

/// @section Vector related forward declarations

template <typename InT, typename InAllocator = gp::memory::DefaultAllocator>
class Vector;
template <typename InT>
using Vector64 = Vector<InT, gp::memory::DefaultAllocator>;
template <typename InT, typename InSizeType = Int32>
class VectorView;
template <typename InT>
using VectorView64 = VectorView<InT, Int64>;
template <typename InT, typename InSizeType = Int32>
using ConstVectorView = VectorView<const InT, InSizeType>;
template <typename InT>
using ConstVectorView64 = VectorView<const InT, Int64>;

/// @section Map related forward declarations

template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, false>>
class SparseMap;
template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, false>>
class CompactMap;
template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, false>>
class Map;
template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, true>>
class SparseMultiMap;
template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, true>>
class CompactMultiMap;
template <
    typename InKeyType,
    typename InValueType,
    typename InSetAllocator = gp::memory::DefaultAllocator,
    typename InKeyFunctions = container::DefaultMapHashtableKeyFunctions<InKeyType, InValueType, true>>
class MultiMap;

template <
    typename InKeyType,
    typename InValueType,
    typename InArrayAllocator = gp::memory::DefaultAllocator,
    typename InSortPredicate = container::Less<typename TypeTraits<InKeyType>::ConstPointerType>>
class SortedMap;
template <
    typename InT,
    typename InArrayAllocator = gp::memory::DefaultAllocator,
    typename InSortPredicate = container::Less<InT>>
class SortedSet;
template <
    typename InT,
    typename InKeyFunctions = container::DefaultKeyFunctions<InT>,
    typename InAllocator = gp::memory::DefaultAllocator>
class SparseSet;
template <
    typename InT,
    typename InKeyFunctions = container::DefaultKeyFunctions<InT>,
    typename InAllocator = gp::memory::DefaultAllocator>
class CompactSet;
template <
    typename InT,
    typename InKeyFunctions = container::DefaultKeyFunctions<InT>,
    typename InAllocator = gp::memory::DefaultAllocator>
class Set;
template <typename InT, typename InSizeType = Int32>
class StridedView;
template <typename InT, typename InSizeType = Int32>
using ConstStridedView = StridedView<const InT, InSizeType>;

}   // namespace gp
