// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/Forward.hpp"
#include "memory/PolymorphicAllocator.hpp"
#include <string>   // required for std::char_traits

// Volountary not inside a specific namespace to avoid excessive nesting for commonly used types like string,
// vector, etc.
namespace gp
{

/// @section Sequences

template <typename T, gp::USize N>
class Array;
template <typename T, typename Alloc = gp::memory::PolymorphicAllocator<T>>
class Vector;
template <typename T, typename Alloc = gp::memory::PolymorphicAllocator<T>>
class List;
template <typename T, typename Alloc = gp::memory::PolymorphicAllocator<T>>
class Deque;
template <typename T, typename Alloc = gp::memory::PolymorphicAllocator<T>>
class ForwardList;

/// @section Specialized Sequences

template <typename T, gp::USize Extent>
class Span;

/// @section Strings

template <
    typename CharT,
    typename Traits = std::char_traits<CharT>,
    typename Alloc = gp::memory::PolymorphicAllocator<CharT>>
class BasicString;

using String = BasicString<char>;
using WString = BasicString<wchar_t>;
using U8String = BasicString<char8_t>;
using U16String = BasicString<char16_t>;

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicStringView;

using StringView = BasicStringView<char>;
using WStringView = BasicStringView<wchar_t>;
using U8StringView = BasicStringView<char8_t>;
using U16StringView = BasicStringView<char16_t>;

/// @section Associative Containers (Ordered)

template <typename K, typename V, typename Compare, typename Alloc>
class Map;
template <typename K, typename V, typename Compare, typename Alloc>
class MultiMap;
template <typename T, typename Compare, typename Alloc>
class Set;
template <typename T, typename Compare, typename Alloc>
class MultiSet;

/// @section Associative Containers (Unordered / Hashed)

template <typename K, typename V, typename Hash, typename Pred, typename Alloc>
class UnorderedMap;
template <typename K, typename V, typename Hash, typename Pred, typename Alloc>
class UnorderedMultiMap;
template <typename T, typename Hash, typename Pred, typename Alloc>
class UnorderedSet;

/// @section Aliases for engine-specific naming conventions

template <typename K, typename V, typename Hash, typename Pred, typename Alloc>
using HashMap = UnorderedMap<K, V, Hash, Pred, Alloc>;

template <typename K, typename V, typename Hash, typename Pred, typename Alloc>
using MultiHashMap = UnorderedMultiMap<K, V, Hash, Pred, Alloc>;

/// @section Other Containers

template <typename T>
class Optional;

}   // namespace gp
