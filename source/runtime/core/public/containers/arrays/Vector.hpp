// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <iterator>

namespace gp
{

/// @brief A dynamic array that can grow and shrink in size.
/// @tparam T The type of elements stored in the vector.
/// @tparam Allocator The allocator type used for memory management.
template <typename T, typename Allocator>
class Vector
{
private:
    template <typename OtherT, typename OtherAllocator>
    friend class Vector;

public:
    using ValueType = T;
    using AllocatorType = Allocator;
    using SizeType = gp::USize;
    using DifferenceType = gp::ISize;
    using Reference = ValueType&;
    using ConstReference = const ValueType&;
    using Pointer = ValueType*;
    using ConstPointer = const ValueType*;
    using Iterator = Pointer;
    using ConstIterator = ConstPointer;
    using ReverseIterator = std::reverse_iterator<Iterator>;
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;
};

}   // namespace gp

namespace gp::concepts
{

namespace detail
{

/// @brief Helper variable template to determine if a type is a gp::Vector.
template <typename T>
constexpr bool IsVectorV = false;

/// @brief Specialization for gp::Vector types.
template <typename T, typename Allocator>
constexpr bool IsVectorV<gp::Vector<T, Allocator>> = true;

}   // namespace detail

/// @brief Concept to check if a type is a gp::Vector.
/// @tparam T The type to check.
template <typename T>
concept IsVector = detail::IsVectorV<std::remove_cvref_t<T>>;

}   // namespace gp::concepts
