// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/RemoveCV.hpp"

namespace GP
{

// clang-format off

/// @brief Forward iterator tag, indicates that an iterator supports at least the operations of a forward iterator
///        (increment, dereference, equality comparison).
struct FForwardIteratorTag                                   {};

/// @brief Bidirectional iterator tag, inherits from forward iterator tag to indicate it has all forward iterator
///        capabilities plus more.
struct FBidirectionalIteratorTag : FForwardIteratorTag       {};

/// @brief Random access iterator tag, inherits from bidirectional iterator tag to indicate it has all bidirectional
///        iterator capabilities plus more.
struct FRandomAccessIteratorTag  : FBidirectionalIteratorTag {};

// clang-format on

/// @brief A simple random access iterator that wraps a raw pointer. Provides basic iterator functionality such as
///        dereferencing, pointer arithmetic, and comparison operators. Designed to be used with contiguous containers
///        like arrays and vectors.
/// @tparam T The type of the elements pointed to by the iterator. Can be const or non-const.
template <typename T>
class TIterator
{
public:
    using IteratorCategory = FRandomAccessIteratorTag;   //<! The category of the iterator.
    using ValueType = TRemoveCV_T<T>;                    //<! The type of the elements pointed to by the iterator.
    using Pointer = T*;                                  //<! The pointer type used to access elements.
    using Reference = T&;                                //<! The reference type used to access elements.
    using DifferenceType = SizeT;                        //<! The type used to represent the difference.

private:
    /// @brief Private constructor for creating an iterator from a raw pointer. Only accessible to friend classes.
    /// @tparam U The type of the pointer (must be compatible with T).
    template <typename U>
    friend class TIterator;

private:
    Pointer m_ptr{ nullptr };   //<! The raw pointer representing the iterator's position.

public:
    /// @brief Default constructor, creates an iterator that does not point to any element (nullptr).
    constexpr TIterator() noexcept = default;

    /// @brief Constructor for creating an iterator from a raw pointer.
    /// @param ptr The raw pointer to the element the iterator should point to.
    constexpr explicit TIterator(Pointer ptr) noexcept
        : m_ptr(ptr)
    {}

    /// @brief Copy constructor for the same type.
    /// @tparam U The type of the other iterator (must be the same as T).
    /// @param other The other iterator to copy from.
    template <typename U>
    requires(CSameAs<U, TRemoveConst_T<T>> && CConst<T>) constexpr TIterator(const TIterator<U>& other) noexcept
        : m_ptr(other.m_ptr)
    {}

public:
    /// @brief Dereference operator to access the element pointed to by the iterator.
    /// @return A reference to the element pointed to by the iterator.
    GP_NODISCARD constexpr Reference operator*() const noexcept { return *m_ptr; }

    /// @brief Arrow operator to access members of the element pointed to by the iterator.
    /// @return A pointer to the element pointed to by the iterator.
    GP_NODISCARD constexpr Pointer operator->() const noexcept { return m_ptr; }

    /// @brief Subscript operator to access elements at an offset from the iterator's current position.
    /// @param n The offset from the current position (can be positive or negative).
    /// @return A reference to the element at the specified offset from the current position.
    /// @warning This operator does not perform bounds checking. The caller must ensure that the resulting position is
    ///          valid.
    GP_NODISCARD constexpr Reference operator[](DifferenceType n) const noexcept { return m_ptr[n]; }

    /// @brief Pre-increment operator to advance the iterator to the next element.
    /// @return A reference to the iterator after it has been advanced.
    constexpr TIterator& operator++() noexcept
    {
        ++m_ptr;
        return *this;
    }

    /// @brief Post-increment operator to advance the iterator to the next element, returning the original position.
    /// @return A copy of the iterator before it was advanced.
    constexpr TIterator operator++(int) noexcept
    {
        TIterator tmp(*this);
        ++m_ptr;
        return tmp;
    }

    /// @brief Pre-decrement operator to move the iterator to the previous element.
    /// @return A reference to the iterator after it has been moved.
    constexpr TIterator& operator--() noexcept
    {
        --m_ptr;
        return *this;
    }

    /// @brief Post-decrement operator to move the iterator to the previous element, returning the original position.
    /// @return A copy of the iterator before it was moved.
    constexpr TIterator operator--(int) noexcept
    {
        TIterator tmp(*this);
        --m_ptr;
        return tmp;
    }

    /// @brief Compound assignment operator to advance the iterator by a specified number of elements.
    /// @param n The number of elements to advance the iterator (can be positive or negative).
    /// @return A reference to the iterator after it has been advanced.
    constexpr TIterator& operator+=(DifferenceType n) noexcept
    {
        m_ptr += n;
        return *this;
    }

    /// @brief Compound assignment operator to move the iterator back by a specified number of elements.
    /// @param n The number of elements to move the iterator back (can be positive or negative).
    /// @return A reference to the iterator after it has been moved back.
    constexpr TIterator& operator-=(DifferenceType n) noexcept
    {
        m_ptr -= n;
        return *this;
    }

    /// @brief Binary addition operator to create a new iterator advanced by a specified number of elements.
    /// @param n The number of elements to advance the iterator (can be positive or negative).
    /// @return A new iterator that is advanced by the specified number of elements from the current position.
    GP_NODISCARD constexpr TIterator operator+(DifferenceType n) const noexcept { return TIterator(m_ptr + n); }

    /// @brief Binary subtraction operator to create a new iterator moved back by a specified number of elements.
    /// @param n The number of elements to move the iterator back (can be positive or negative).
    /// @return A new iterator that is moved back by the specified number of elements from the current position.
    GP_NODISCARD constexpr TIterator operator-(DifferenceType n) const noexcept { return TIterator(m_ptr - n); }

    /// @brief Binary addition operator to create a new iterator advanced by a specified number of elements, with the
    ///        iterator on the right-hand side.
    /// @param n The number of elements to advance the iterator (can be positive or negative).
    /// @param it The iterator to advance (must be the same type as the current iterator).
    /// @return A new iterator that is advanced by the specified number of elements from the position of the provided
    ///         iterator.
    GP_NODISCARD friend constexpr TIterator operator+(DifferenceType n, const TIterator& it) noexcept
    {
        return TIterator(it.m_ptr + n);
    }

    /// @brief Binary subtraction operator to calculate the distance between two iterators.
    /// @param lhs The left-hand side iterator (must be the same type as the current iterator).
    /// @param rhs The right-hand side iterator (must be the same type as the current iterator).
    /// @return The number of elements between the two iterators (positive if lhs is ahead of rhs, negative if lhs is
    ///         behind rhs).
    GP_NODISCARD constexpr DifferenceType operator-(const TIterator& lhs, const TIterator& rhs) noexcept
    {
        return lhs.m_ptr - rhs.m_ptr;
    }

    /// @brief Comparison operators to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the comparison condition is satisfied, false otherwise.
    GP_NODISCARD constexpr bool operator==(const TIterator& other) const noexcept { return m_ptr == other.m_ptr; }

    /// @brief Inequality operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the iterators point to different positions, false if they point to the same position.
    GP_NODISCARD constexpr bool operator!=(const TIterator& other) const noexcept { return m_ptr != other.m_ptr; }

    /// @brief Less-than operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the current iterator points to a position before the other iterator, false otherwise.
    GP_NODISCARD constexpr bool operator<(const TIterator& other) const noexcept { return m_ptr < other.m_ptr; }

    /// @brief Less-than-or-equal operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the current iterator points to a position before or the same as the other iterator, false
    ///         otherwise.
    GP_NODISCARD constexpr bool operator<=(const TIterator& other) const noexcept { return m_ptr <= other.m_ptr; }

    /// @brief Greater-than operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the current iterator points to a position after the other iterator, false otherwise.
    GP_NODISCARD constexpr bool operator>(const TIterator& other) const noexcept { return m_ptr > other.m_ptr; }

    /// @brief Greater-than-or-equal operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return True if the current iterator points to a position after or the same as the other iterator, false
    ///         otherwise.
    GP_NODISCARD constexpr bool operator>=(const TIterator& other) const noexcept { return m_ptr >= other.m_ptr; }

    /// @brief Three-way comparison operator to compare the positions of two iterators.
    /// @param other The other iterator to compare with (must be the same type as the current iterator).
    /// @return A std::strong_ordering value indicating the relative positions of the two iterators (less, equal, or
    ///         greater).
    GP_NODISCARD constexpr auto operator<=>(const TIterator& other) const noexcept = default;

public:
    /// @brief Accessor to retrieve the raw pointer from the iterator.
    /// @return The raw pointer representing the iterator's position.
    GP_NODISCARD constexpr Pointer GetPtr() const noexcept { return m_ptr; }
};

/// @brief Convenience type alias for a const iterator. Provides the same functionality as TIterator but with a
///        const-qualified value type.
template <typename T>
using TConstIterator = TIterator<const T>;

}   // namespace GP
